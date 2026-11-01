#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

// Free-draw graphic EQ (output-stage tone shaping, base rate).
// 31 dots on the ISO 1/3-octave centres, 20 Hz to 20 kHz, drive a bank of 61 peaking
// filters (a peak on each dot and one halfway between neighbours). The eqThird params
// hold the drawn curve, one level per dot. A Fitter turns it into the 61 peak gains at
// the session's rate, so the bank's summed response follows the drawing; overlapping
// peaks would otherwise pile up past what was drawn. The
// processor fits on the audio thread whenever the drawing changes, and the editor
// runs the same fit to draw what is heard.
// The two end dots are not peaks: dot 0 is a high-pass and dot 30 a low-pass (see
// EQ_HP_BAND), so the curve never hooks back to 0 dB past the ends of the drawing.
namespace DSPConstants
{
    constexpr int    EQ_NUM_BANDS = 31;                       // dots (params) in the drawing
    // The bank has a peak on every dot and one halfway (in log frequency) between
    // neighbours: 61 peaks, 1/6 octave apart.
    constexpr int    EQ_NUM_PEAKS = 2 * EQ_NUM_BANDS - 1;
    // The end dots are 12 dB/oct Butterworth filters. At 0 dB (or above) each is off;
    // dragging one down moves its cutoff in by an octave per EQ_EDGE_DB_PER_OCTAVE, so
    // at -24 dB the high-pass sits at 320 Hz and the low-pass at 1.25 kHz. They fade
    // in and out over EQ_GAIN_SMOOTH_TIME_S (a dry/filtered blend), so switching one
    // on or off cannot click, and an off filter is exactly transparent.
    constexpr int    EQ_HP_BAND = 0;
    constexpr int    EQ_LP_BAND = EQ_NUM_BANDS - 1;
    constexpr double EQ_EDGE_DB_PER_OCTAVE = 6.0;
    constexpr double EQ_EDGE_Q = 0.70710678118654752;
    // A drawn cut reaches -24 dB, a boost +12 dB...
    constexpr float  EQ_DRAW_MIN_DB = -24.0f;
    constexpr float  EQ_DRAW_MAX_DB =  12.0f;
    // ...and a band's own (fitted) gain has headroom past that, because holding a
    // neighbour at 0 dB next to a deep cut takes a band deeper than the cut itself.
    constexpr float  EQ_BAND_MIN_DB = -36.0f;
    constexpr float  EQ_BAND_MAX_DB =  18.0f;
    // About 0.4 octave wide, measured against the peaks' 1/6-octave spacing. Chosen
    // with the 61-peak bank in a numeric sweep: peaks this close, with four fit points
    // per dot gap, cut the ringing around a steep jump from ~25 dB of excess wiggle (31
    // peaks, Q 2.5) to ~7 dB in the scratch harness (~8.6 dB in the in-repo test), and the dot error beside a jump from 5 to under 1 dB,
    // while a one-dot notch still reaches its depth. Narrower rings less but cannot
    // reach a notch; wider smears the cuts.
    // Near Nyquist each peak is built with GraphicEqCurve::bandQ, which keeps this
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
    using Bands = std::array<float, DSPConstants::EQ_NUM_BANDS>;       // the drawing: one level per dot
    using Gains = std::array<float, DSPConstants::EQ_NUM_PEAKS>;       // the bank: one gain per peak

    /** Centre of peak k: even k sit on a dot, odd k halfway (geometric mean) between two. */
    inline float peakFreq(int k) noexcept
    {
        const float lo = DSPConstants::EQ_FREQS[k / 2];
        if (k % 2 == 0)
            return lo;
        return (float) std::sqrt((double) lo * (double) DSPConstants::EQ_FREQS[k / 2 + 1]);
    }

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

    inline bool peakUsable(int k, double sampleRate) noexcept
    {
        return peakFreq(k) < sampleRate * DSPConstants::EQ_MAX_FREQ_RATIO;
    }

    inline bool isEdgeBand(int band) noexcept
    {
        return band == DSPConstants::EQ_HP_BAND || band == DSPConstants::EQ_LP_BAND;
    }

    // --- The end dots: high-pass (dot 0) and low-pass (dot 30) ---------------------

    /** Where an end filter rests when off, and the furthest out it can go: 20 Hz for
        the high-pass; 20 kHz for the low-pass, or EQ_MAX_FREQ_RATIO x rate if lower. */
    inline double edgeRestHz(bool highPass, double sampleRate) noexcept
    {
        using namespace DSPConstants;
        return highPass ? (double) EQ_FREQS[EQ_HP_BAND]
                        : std::fmin((double) EQ_FREQS[EQ_LP_BAND], sampleRate * EQ_MAX_FREQ_RATIO);
    }

    /** The cutoff an end dot asks for: an octave in per EQ_EDGE_DB_PER_OCTAVE below
        0 dB, from 20 Hz up (high-pass) or 20 kHz down (low-pass). */
    inline double edgeCutoffHz(bool highPass, float dotDb, double sampleRate) noexcept
    {
        using namespace DSPConstants;
        const double d = std::fmax((double) EQ_DRAW_MIN_DB, std::fmin(0.0, (double) dotDb));
        const double octaves = -d / EQ_EDGE_DB_PER_OCTAVE;
        if (highPass)
            return EQ_FREQS[EQ_HP_BAND] * std::exp2(octaves);
        return std::fmin(edgeRestHz(false, sampleRate), EQ_FREQS[EQ_LP_BAND] * std::exp2(-octaves));
    }

    /** True while an end dot's filter plays: the dot is below 0 dB (by more than half
        a param step) and, for the low-pass, its cutoff is below where it rests. */
    inline bool edgeOn(bool highPass, float dotDb, double sampleRate) noexcept
    {
        if (! (dotDb < -0.005f))
            return false;
        return highPass || edgeCutoffHz(false, dotDb, sampleRate) < edgeRestHz(false, sampleRate);
    }

    /** RBJ Butterworth high- or low-pass, normalised by a0: { b0, b1, b2, a1, a2 }.
        The DSP and the model both build the end filters with this. */
    inline void edgeCoeffs(bool highPass, double cutoffHz, double sampleRate, double (&c)[5]) noexcept
    {
        const double w0 = 6.283185307179586 * cutoffHz / sampleRate;
        const double cw = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * DSPConstants::EQ_EDGE_Q);
        const double a0inv = 1.0 / (1.0 + alpha);
        const double bEdge = highPass ? 0.5 * (1.0 + cw) : 0.5 * (1.0 - cw);
        c[0] = bEdge * a0inv;
        c[1] = (highPass ? -2.0 : 2.0) * bEdge * a0inv;
        c[2] = c[0];
        c[3] = -2.0 * cw * a0inv;
        c[4] = (1.0 - alpha) * a0inv;
    }

    // Magnitude (dB) of a normalised biquad at f.
    inline double biquadDb(const double (&c)[5], double f, double sampleRate) noexcept
    {
        const double w = 6.283185307179586 * f / sampleRate;
        const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
        const double nr = c[0] + c[1] * c1 + c[2] * c2, ni = -(c[1] * s1 + c[2] * s2);
        const double dr = 1.0 + c[3] * c1 + c[4] * c2, di = -(c[3] * s1 + c[4] * s2);
        return 10.0 * std::log10((nr * nr + ni * ni) / (dr * dr + di * di) + 1.0e-300);
    }

    /** The end filters' response (dB) at f for a drawing, as the DSP plays it once
        settled. */
    inline double edgeDb(const std::array<float, DSPConstants::EQ_NUM_BANDS>& drawnDb, double f,
                         double sampleRate) noexcept
    {
        using namespace DSPConstants;
        double sum = 0.0;
        for (const bool hp : { true, false })
        {
            const float dot = drawnDb[(size_t) (hp ? EQ_HP_BAND : EQ_LP_BAND)];
            if (edgeOn(hp, dot, sampleRate))
            {
                double c[5];
                edgeCoeffs(hp, edgeCutoffHz(hp, dot, sampleRate), sampleRate, c);
                sum += biquadDb(c, f, sampleRate);
            }
        }
        return sum;
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
    inline double responseDb(const Gains& gainsDb, double f,
                             double sampleRate = DSPConstants::EQ_FIT_SAMPLE_RATE) noexcept
    {
        double sum = 0.0;
        for (int m = 0; m < DSPConstants::EQ_NUM_PEAKS; ++m)
            if (gainsDb[(size_t) m] != 0.0f && peakUsable(m, sampleRate))
                sum += peakDb(f, peakFreq(m), bandQ(peakFreq(m), sampleRate),
                              gainsDb[(size_t) m], sampleRate);
        return sum;
    }

    /** What is heard (dB) at f: the peaking bank's fitted gains plus the end filters
        the drawing asks for. The editor draws this. */
    inline double totalDb(const Gains& gainsDb, const Bands& drawnDb, double f,
                          double sampleRate = DSPConstants::EQ_FIT_SAMPLE_RATE) noexcept
    {
        return responseDb(gainsDb, f, sampleRate) + edgeDb(drawnDb, f, sampleRate);
    }

    /** Turns a drawn curve into band gains at one sample rate. prepare() builds the
        solver (about a millisecond; allocates its ~300 kB of tables the first
        time, so not for the audio thread); fit() is allocation-free and cheap (about
        97 microseconds in Release), so the processor runs it on the audio thread whenever the
        drawing changes. fit() before any prepare() returns flat gains. */
    class Fitter
    {
    public:
        static constexpr int N = DSPConstants::EQ_NUM_PEAKS;
        // The fit is checked at every dot and at three points between neighbours (four
        // per dot gap, every 1/12 octave), so a shape cannot ripple unseen between peaks.
        static constexpr int GRID_PER_GAP = 4;
        static constexpr int G = GRID_PER_GAP * (DSPConstants::EQ_NUM_BANDS - 1) + 1;

        static double gridFreq(int j) noexcept
        {
            const int i = j / GRID_PER_GAP;
            if (i >= DSPConstants::EQ_NUM_BANDS - 1)
                return DSPConstants::EQ_FREQS[DSPConstants::EQ_NUM_BANDS - 1];
            const double t = (double) (j % GRID_PER_GAP) / GRID_PER_GAP;
            return DSPConstants::EQ_FREQS[i] * std::pow((double) DSPConstants::EQ_FREQS[i + 1] / DSPConstants::EQ_FREQS[i], t);
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

            // The end dots set the end filters, but the 20 Hz and 20 kHz peaks stay in
            // the bank: the fit uses them to hold the neighbouring level out to the ends.
            for (int m = 0; m < N; ++m)
                usable[m] = peakUsable(m, rate);
            int firstPeak = N, lastPeak = -1;
            for (int m = 0; m < N; ++m)
                if (usable[m])
                {
                    firstPeak = std::min(firstPeak, m);
                    lastPeak = m;
                }
            for (int j = 0; j < G; ++j)
            {
                // A grid point the bank can be heard at: below Nyquist, with a margin.
                gridOn[j] = gridFreq(j) < rate * EQ_MAX_FREQ_RATIO;
                // Dots and the points between them count the same, which keeps a
                // wandering drawing within ~1 dB of its lines between dots.
                // Past the outermost peaks no peak can hold a level, so those
                // points count for less; at full weight they pulled the last band
                // ~1.7 dB past its dot (a 16 kHz boost at 44.1 kHz). The margin keeps
                // a grid point that sits on the outermost peak counted as inside.
                const double fj = gridFreq(j);
                const bool outside = firstPeak > lastPeak
                                  || fj < peakFreq(firstPeak) * (1.0 - 1.0e-6)
                                  || fj > peakFreq(lastPeak) * (1.0 + 1.0e-6);
                weight[j] = gridOn[j] ? (outside ? 0.3 : 1.0) : 0.0;
                for (int m = 0; m < N; ++m)
                {
                    terms[j][m] = usable[m] ? PeakTerms(fj, peakFreq(m), bandQ(peakFreq(m), rate), rate)
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
        Gains fit(const Bands& drawnDb) const noexcept
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

            Gains gains {};
            if (t == nullptr)
                return gains;
            const auto& usable = t->usable;
            const auto& gridOn = t->gridOn;
            const auto& terms = t->terms;
            const auto& P = t->P;
            // The end dots set the end filters, not a level: the bank holds the
            // neighbouring dots' levels out to the ends.
            double level[EQ_NUM_BANDS];
            for (int i = 0; i < EQ_NUM_BANDS; ++i)
                level[i] = clampDb(drawnDb[(size_t) (i == EQ_HP_BAND ? EQ_HP_BAND + 1
                                                     : i == EQ_LP_BAND ? EQ_LP_BAND - 1 : i)]);
            double grid[G];
            bool flat = true;
            for (int j = 0; j < G; ++j)
            {
                const int i = std::min(j / GRID_PER_GAP, EQ_NUM_BANDS - 2);
                const double u = (double) (j - i * GRID_PER_GAP) / GRID_PER_GAP;
                grid[j] = level[i] * (1.0 - u) + level[i + 1] * u;
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
    inline Gains fit(const Bands& drawnDb, double sampleRate = DSPConstants::EQ_FIT_SAMPLE_RATE)
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
        // v2.3 had no end filters: leave both off. The bank holds dots 1 and 29 out
        // to the ends, which is close to what the old curve did there.
        drawn[(size_t) EQ_HP_BAND] = 0.0f;
        drawn[(size_t) EQ_LP_BAND] = 0.0f;
        return drawn;
    }
}
