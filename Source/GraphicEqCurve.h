#pragma once

#include <array>
#include <cmath>
#include <memory>

// Free-draw graphic EQ (output-stage tone shaping, base rate).
// A bank of 31 peaking filters on the ISO 1/3-octave centres, 20 Hz to 20 kHz. The
// eqThird params hold the drawn curve, one level per band centre. A Fitter turns it
// into band gains at the session's rate, so the bank's summed response follows the
// drawing; overlapping peaks would otherwise pile up past what was drawn. The
// processor fits on the audio thread whenever the drawing changes, and the editor
// runs the same fit to draw what is heard.
namespace DSPConstants
{
    constexpr int    EQ_NUM_BANDS = 31;
    // A drawn cut reaches -24 dB, a boost +12 dB...
    constexpr float  EQ_DRAW_MIN_DB = -24.0f;
    constexpr float  EQ_DRAW_MAX_DB =  12.0f;
    // ...and a band's own (fitted) gain has headroom past that, because holding a
    // neighbour at 0 dB next to a deep cut takes a band deeper than the cut itself.
    constexpr float  EQ_BAND_MIN_DB = -36.0f;
    constexpr float  EQ_BAND_MAX_DB =  18.0f;
    // About 0.4 octave wide: narrow enough for a 1/3-octave notch to reach -24 dB,
    // wide enough that a broad plateau ripples no more than ~3.5 dB between centres.
    // Near Nyquist each band is built with GraphicEqCurve::bandQ, which keeps this
    // width in octaves.
    constexpr float  EQ_Q = 3.5f;
    constexpr float  EQ_FLAT_EPS_DB = 0.05f;                  // Below this |gain|, treat as flat
    constexpr double EQ_GAIN_SMOOTH_TIME_S = 0.03;            // 30ms gain ramp (kills zipper on drag)
    constexpr int    EQ_SUBBLOCK = 32;                        // samples per coefficient update
    // A band that has gone flat keeps running this long, so its ringing (about
    // 55 ms to -40 dB for the 20 Hz band) decays before its memory is cleared and it
    // stops; clearing it while it rings would click.
    constexpr double EQ_IDLE_HOLD_S = 0.5;
    // Highest usable band centre, as a fraction of the base sample rate. A peaking
    // biquad's alpha term goes negative once its centre reaches Nyquist
    // (sin(omega) <= 0), which pushes the poles onto/outside the unit circle and
    // makes the band diverge. Bands above this ratio are skipped entirely: they are
    // out of the reproducible band anyway. 0.45 keeps a margin below the 0.5 hard
    // limit. At 44.1 kHz the 20 kHz band drops out; at 48 kHz and up all 31 run.
    constexpr double EQ_MAX_FREQ_RATIO = 0.45;
    // ISO 266 nominal 1/3-octave centres.
    constexpr float  EQ_FREQS[EQ_NUM_BANDS] = {
        20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f,
        200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f, 1250.0f,
        1600.0f, 2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f,
        10000.0f, 12500.0f, 16000.0f, 20000.0f
    };
    // The fit runs at the session's rate, so the bands the DSP drops there are fitted
    // around. This is the rate used before one is known.
    constexpr double EQ_FIT_SAMPLE_RATE = 48000.0;

    // The v2.3 bank (state version < 4): 12 bands, ±12 dB, params eqBand0..11.
    // Only the state migration reads these.
    constexpr int    EQ_LEGACY_NUM_BANDS = 12;
    constexpr float  EQ_LEGACY_Q = 1.4f;
    constexpr float  EQ_LEGACY_FREQS[EQ_LEGACY_NUM_BANDS] = {
        30.0f, 55.0f, 100.0f, 180.0f, 320.0f, 560.0f,
        1000.0f, 1800.0f, 3200.0f, 5600.0f, 9000.0f, 16000.0f
    };
}

namespace GraphicEqCurve
{
    using Bands = std::array<float, DSPConstants::EQ_NUM_BANDS>;

    /** The Q to build band `centre` with at sampleRate, so its bandwidth in octaves
        is EQ_Q's analog bandwidth at every centre. The bilinear transform narrows a
        constant-Q peak as it nears Nyquist; at 44.1 kHz that scalloped a drawn air
        lift by ~4 dB between the top bands. RBJ's prewarped bandwidth form,
        alpha = sin(w0) sinh(ln2/2 BW w0/sin(w0)), with BW taken from EQ_Q, undoes it;
        at low centres it is EQ_Q itself. The DSP and the model both use this. */
    inline double bandQ(double centre, double sampleRate) noexcept
    {
        const double omega = 6.283185307179586 * centre / sampleRate;
        const double halfBwLn = std::asinh(1.0 / (2.0 * DSPConstants::EQ_Q));   // ln2/2 * BW
        return 1.0 / (2.0 * std::sinh(halfBwLn * omega / std::sin(omega)));
    }

    inline bool bandUsable(int band, double sampleRate) noexcept
    {
        return DSPConstants::EQ_FREQS[band] < sampleRate * DSPConstants::EQ_MAX_FREQ_RATIO;
    }

    // An RBJ peaking biquad's |H(f)|^2 is (K + L A^2) / (K + L / A^2), with A^2 the
    // linear gain, K = |1 - 2cos(w0) z^-1 + z^-2|^2 = 4 (cos w - cos w0)^2 and
    // L = 4 alpha^2 sin^2 w, which depend only on f, the centre, Q and the rate.
    // Keeping them lets the fit evaluate a band at any gain without a trig call. K is
    // taken as 16 sin^2((w+w0)/2) sin^2((w-w0)/2): expanded as a sum of cosines it
    // cancels to nothing at low frequencies and high rates, and the 20 Hz band at
    // 192 kHz was fitted 4.6 dB off.
    struct PeakTerms
    {
        double K = 0.0, L = 0.0;

        PeakTerms() = default;
        PeakTerms(double f, double centre, double q, double sampleRate) noexcept
        {
            constexpr double twoPi = 6.283185307179586;
            const double w0 = twoPi * centre / sampleRate;
            const double w = twoPi * f / sampleRate;
            const double alpha = std::sin(w0) / (2.0 * q);
            const double sSum = std::sin(0.5 * (w + w0)), sDiff = std::sin(0.5 * (w - w0));
            const double sw = std::sin(w);
            K = 16.0 * sSum * sSum * sDiff * sDiff;
            L = 4.0 * alpha * alpha * sw * sw;
        }

        double ratio(double A2) const noexcept { return (K + L * A2) / (K + L / A2); }
    };

    // Magnitude (dB) at f of the RBJ peaking biquad writePeakFilterCoeffs builds.
    inline double peakDb(double f, double centre, double q, double gainDb, double sampleRate) noexcept
    {
        return 10.0 * std::log10(PeakTerms(f, centre, q, sampleRate).ratio(std::pow(10.0, gainDb / 20.0)));
    }

    // The bank's response (dB) at f, skipping the bands the processor drops at
    // this rate.
    inline double responseDb(const Bands& gainsDb, double f,
                             double sampleRate = DSPConstants::EQ_FIT_SAMPLE_RATE) noexcept
    {
        double sum = 0.0;
        for (int m = 0; m < DSPConstants::EQ_NUM_BANDS; ++m)
            if (gainsDb[(size_t) m] != 0.0f && bandUsable(m, sampleRate))
                sum += peakDb(f, DSPConstants::EQ_FREQS[m], bandQ(DSPConstants::EQ_FREQS[m], sampleRate),
                              gainsDb[(size_t) m], sampleRate);
        return sum;
    }

    /** Turns a drawn curve into band gains at one sample rate. prepare() builds the
        solver (well under a millisecond; allocates its ~75 kB of tables the first
        time, so not for the audio thread); fit() is allocation-free and cheap (tens
        of microseconds), so the processor runs it on the audio thread whenever the
        drawing changes. fit() before any prepare() returns flat gains. */
    class Fitter
    {
    public:
        static constexpr int N = DSPConstants::EQ_NUM_BANDS;
        // The fit is checked at every centre and halfway between neighbours, so a
        // broad shape cannot ripple unseen between centres.
        static constexpr int G = 2 * N - 1;

        static double gridFreq(int j) noexcept
        {
            const int i = j / 2;
            if (j % 2 == 0)
                return DSPConstants::EQ_FREQS[i];
            return std::sqrt((double) DSPConstants::EQ_FREQS[i] * DSPConstants::EQ_FREQS[i + 1]);
        }

        double sampleRate() const noexcept { return rate; }

        void prepare(double sampleRate)
        {
            using namespace DSPConstants;
            if (t == nullptr)
                t = std::make_unique<Tables>();
            auto& usable = t->usable;
            auto& gridOn = t->gridOn;
            auto& weight = t->weight;
            auto& terms = t->terms;
            auto& B = t->B;
            auto& M = t->M;
            auto& P = t->P;
            rate = sampleRate > 0.0 ? sampleRate : EQ_FIT_SAMPLE_RATE;
            constexpr double refDb = 12.0;
            constexpr double lambda = 0.02;   // keeps the solve from trading huge opposite gains
            const double refA2 = std::pow(10.0, refDb / 20.0);

            for (int m = 0; m < N; ++m)
                usable[m] = bandUsable(m, rate);
            for (int j = 0; j < G; ++j)
            {
                // A grid point the bank can be heard at: below Nyquist, with a margin.
                gridOn[j] = gridFreq(j) < rate * EQ_MAX_FREQ_RATIO;
                // Midpoints count for less than centres: between two very different
                // dots (a steep cut) no bank of peaks can pass through the halfway
                // level, and an equal weight there pulls the cut past the dots. 0.3
                // keeps a one-octave cut within ~1.5 dB of its dots and a broad
                // plateau within ~3.5 dB between them.
                weight[j] = gridOn[j] ? (j % 2 == 0 ? 1.0 : 0.3) : 0.0;
                for (int m = 0; m < N; ++m)
                {
                    terms[j][m] = usable[m] ? PeakTerms(gridFreq(j), EQ_FREQS[m], bandQ(EQ_FREQS[m], rate), rate)
                                            : PeakTerms();
                    // Column m of the Jacobian: band m's response per dB at the reference gain.
                    B[j][m] = usable[m] && gridOn[j] ? 10.0 * std::log10(terms[j][m].ratio(refA2)) / refDb : 0.0;
                }
            }

            // Weighted least squares P = (B'WB + lambda I)^-1 B'W. A band the DSP drops
            // has a zero column, so lambda holds it at 0 dB. Gauss-Jordan with partial
            // pivoting on [B'WB + lambda I | I].
            for (int r = 0; r < N; ++r)
                for (int c = 0; c < N; ++c)
                {
                    double sum = 0.0;
                    for (int j = 0; j < G; ++j)
                        sum += weight[j] * B[j][r] * B[j][c];
                    M[r][c] = sum + (r == c ? lambda : 0.0);
                    M[r][N + c] = (r == c ? 1.0 : 0.0);
                }
            for (int col = 0; col < N; ++col)
            {
                int pivot = col;
                for (int r = col + 1; r < N; ++r)
                    if (std::abs(M[r][col]) > std::abs(M[pivot][col]))
                        pivot = r;
                if (pivot != col)
                    for (int c = 0; c < 2 * N; ++c)
                        std::swap(M[col][c], M[pivot][c]);
                const double inv = 1.0 / M[col][col];
                for (int c = 0; c < 2 * N; ++c)
                    M[col][c] *= inv;
                for (int r = 0; r < N; ++r)
                    if (r != col && M[r][col] != 0.0)
                    {
                        const double k = M[r][col];
                        for (int c = 0; c < 2 * N; ++c)
                            M[r][c] -= k * M[col][c];
                    }
            }
            for (int m = 0; m < N; ++m)
                for (int j = 0; j < G; ++j)
                {
                    double sum = 0.0;
                    for (int k = 0; k < N; ++k)
                        sum += M[m][N + k] * B[j][k];
                    P[m][j] = sum * weight[j];
                }
        }

        /** Band gains whose summed response follows a drawn curve, given as one
            level (dB) per band centre and taken as a straight line in log frequency
            between centres. Levels are clamped to the drawing range. Bands the DSP
            drops at this rate stay at 0 dB, and a flat drawing gives exactly flat
            gains, so the bank stays bypassed. Call prepare() first. */
        Bands fit(const Bands& drawnDb) const noexcept
        {
            using namespace DSPConstants;
            // A param set to 0 dB can read back a hair off it: the range maps 0 to
            // -24 + 0.01 x 2400, which a fused multiply-add (Apple silicon) leaves at
            // ~5e-7. Anything under half a param step is 0, so a flat drawing is
            // exactly flat on every platform.
            auto clampDb = [](float v)
            {
                if (std::abs(v) < 0.005f)
                    return 0.0;
                return (double) std::fmin(EQ_DRAW_MAX_DB, std::fmax(EQ_DRAW_MIN_DB, v));
            };

            Bands gains {};
            if (t == nullptr)
                return gains;
            const auto& usable = t->usable;
            const auto& gridOn = t->gridOn;
            const auto& terms = t->terms;
            const auto& P = t->P;
            double grid[G];
            bool flat = true;
            for (int j = 0; j < G; ++j)
            {
                const int i = j / 2;
                grid[j] = (j % 2 == 0) ? clampDb(drawnDb[(size_t) i])
                                       : 0.5 * (clampDb(drawnDb[(size_t) i]) + clampDb(drawnDb[(size_t) i + 1]));
                flat = flat && grid[j] == 0.0;
            }
            if (flat)
                return gains;

            // Newton steps with a fixed Jacobian: the peaking response is close to
            // linear in its dB gain, so a handful of corrections absorbs what isn't.
            double A2[N];
            for (int iter = 0; iter < 5; ++iter)
            {
                for (int m = 0; m < N; ++m)
                    A2[m] = std::pow(10.0, gains[(size_t) m] / 20.0);
                double residual[G];
                for (int j = 0; j < G; ++j)
                {
                    if (! gridOn[j]) { residual[j] = 0.0; continue; }
                    // Product of the band ratios, so one log per grid point.
                    double product = 1.0;
                    for (int m = 0; m < N; ++m)
                        if (usable[m] && gains[(size_t) m] != 0.0f)
                            product *= terms[j][m].ratio(A2[m]);
                    residual[j] = grid[j] - 10.0 * std::log10(product);
                }
                for (int m = 0; m < N; ++m)
                {
                    double step = 0.0;
                    for (int j = 0; j < G; ++j)
                        step += P[m][j] * residual[j];
                    const double g = gains[(size_t) m] + step;
                    gains[(size_t) m] = usable[m] ? (float) std::fmin(EQ_BAND_MAX_DB, std::fmax(EQ_BAND_MIN_DB, g))
                                                  : 0.0f;
                }
            }
            return gains;
        }

    private:
        struct Tables
        {
            bool usable[N] {};
            bool gridOn[G] {};
            double weight[G] {};
            PeakTerms terms[G][N];
            double B[G][N] {};
            double M[N][2 * N] {};
            double P[N][G] {};
        };
        double rate = 0.0;
        std::unique_ptr<Tables> t;
    };

    /** fit() at a given rate, for callers off the audio thread (tests, tools). Keeps
        one prepared Fitter per thread, rebuilt when the rate changes. */
    inline Bands fit(const Bands& drawnDb, double sampleRate = DSPConstants::EQ_FIT_SAMPLE_RATE)
    {
        thread_local Fitter fitter;
        const double rate = sampleRate > 0.0 ? sampleRate : DSPConstants::EQ_FIT_SAMPLE_RATE;
        if (fitter.sampleRate() != rate)
            fitter.prepare(rate);
        return fitter.fit(drawnDb);
    }

    /** The drawing that reproduces a v2.3 curve (12 bands, Q 1.4, at 48 kHz): its
        response at each band centre, clamped to the drawing range. Between centres
        the new bank follows straight lines, so an old peak midway between two
        centres comes back up to ~1.5 dB lower. */
    inline Bands legacyDrawing(const float (&legacyGainsDb)[DSPConstants::EQ_LEGACY_NUM_BANDS])
    {
        using namespace DSPConstants;
        Bands drawn {};
        for (int k = 0; k < EQ_NUM_BANDS; ++k)
        {
            double sum = 0.0;
            for (int m = 0; m < EQ_LEGACY_NUM_BANDS; ++m)
                if (legacyGainsDb[m] != 0.0f)
                    sum += peakDb(EQ_FREQS[k], EQ_LEGACY_FREQS[m], EQ_LEGACY_Q,
                                  legacyGainsDb[m], EQ_FIT_SAMPLE_RATE);
            drawn[(size_t) k] = std::round((float) std::fmin(EQ_DRAW_MAX_DB, std::fmax(EQ_DRAW_MIN_DB, sum)) * 100.0f) / 100.0f;
        }
        return drawn;
    }
}
