#pragma once

#include <array>
#include <cmath>

// Free-draw graphic EQ (output-stage tone shaping, base rate).
// A bank of 31 peaking filters on the ISO 1/3-octave centres, 20 Hz to 20 kHz. The
// drawn curve is a target level per band; GraphicEqCurve::fit turns it into band
// gains so the bank's summed response follows the drawing, since overlapping
// peaks would otherwise pile up past what was drawn. Engages whenever any band is
// non-flat.
namespace DSPConstants
{
    constexpr int    EQ_NUM_BANDS = 31;
    // A drawn cut reaches -24 dB, a boost +12 dB...
    constexpr float  EQ_DRAW_MIN_DB = -24.0f;
    constexpr float  EQ_DRAW_MAX_DB =  12.0f;
    // ...and a band's own gain has headroom past that, because holding a neighbour
    // at 0 dB next to a deep cut takes a band deeper than the cut itself.
    constexpr float  EQ_BAND_MIN_DB = -30.0f;
    constexpr float  EQ_BAND_MAX_DB =  18.0f;
    // About 0.4 octave wide: narrow enough for a 1/3-octave notch to reach -24 dB,
    // wide enough that a broad plateau ripples no more than ~3.5 dB between centres.
    // Near Nyquist each band is built with GraphicEqCurve::bandQ, which keeps this
    // width in octaves.
    constexpr float  EQ_Q = 3.5f;
    constexpr float  EQ_FLAT_EPS_DB = 0.05f;                  // Below this |gain|, treat as flat
    constexpr double EQ_GAIN_SMOOTH_TIME_S = 0.03;            // 30ms gain ramp (kills zipper on drag)
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
    // The fit runs at the host rate, so the bands the DSP drops there and the
    // narrowing of the top bands near Nyquist are part of it. This is the rate used
    // before a host rate is known.
    constexpr double EQ_FIT_SAMPLE_RATE = 48000.0;

    // The v2.3 bank (state version < 4): 12 bands, ±12 dB, params eqBand0..11.
    // Only migrateState reads these.
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

    // Magnitude (dB) at f of the RBJ peaking biquad writePeakFilterCoeffs builds.
    inline double peakDb(double f, double centre, double q, double gainDb, double sampleRate) noexcept
    {
        constexpr double twoPi = 6.283185307179586;
        const double A = std::pow(10.0, gainDb / 40.0);
        const double omega = twoPi * centre / sampleRate;
        const double alpha = std::sin(omega) / (2.0 * q);
        const double c = -2.0 * std::cos(omega);
        const double b0 = 1.0 + alpha * A, b2 = 1.0 - alpha * A;
        const double a0 = 1.0 + alpha / A, a2 = 1.0 - alpha / A;
        const double w = twoPi * f / sampleRate;
        const double cw = std::cos(w), c2w = std::cos(2.0 * w);
        const double num = b0 * b0 + c * c + b2 * b2 + 2.0 * c * (b0 + b2) * cw + 2.0 * b0 * b2 * c2w;
        const double den = a0 * a0 + c * c + a2 * a2 + 2.0 * c * (a0 + a2) * cw + 2.0 * a0 * a2 * c2w;
        return 10.0 * std::log10(num / den);
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

    inline bool bandUsable(int band, double sampleRate) noexcept
    {
        return DSPConstants::EQ_FREQS[band] < sampleRate * DSPConstants::EQ_MAX_FREQ_RATIO;
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

    namespace detail
    {
        // The fit is checked at every centre and halfway between neighbours, so a
        // broad shape cannot ripple unseen between centres.
        constexpr int kGrid = 2 * DSPConstants::EQ_NUM_BANDS - 1;

        inline double gridFreq(int j) noexcept
        {
            const int i = j / 2;
            if (j % 2 == 0)
                return DSPConstants::EQ_FREQS[i];
            return std::sqrt((double) DSPConstants::EQ_FREQS[i] * DSPConstants::EQ_FREQS[i + 1]);
        }

        // Midpoints count for less than centres: between two very different dots (a
        // steep cut) no bank of peaks can pass through the halfway level, and an
        // equal weight there pulls the cut past the dots.
        // 0.3 keeps a one-octave cut within ~1.5 dB of its dots and a broad plateau
        // within ~3.5 dB between them (1.0: cuts 4 dB past the dots near 20 kHz).
        constexpr double kMidWeight = 0.3;
        inline double gridWeight(int j) noexcept { return j % 2 == 0 ? 1.0 : kMidWeight; }

        // A grid point the bank can be heard at: below Nyquist, with a margin.
        inline bool gridUsable(int j, double sampleRate) noexcept
        {
            return gridFreq(j) < sampleRate * DSPConstants::EQ_MAX_FREQ_RATIO;
        }

        // Weighted least-squares solver P = (B'WB + lambda I)^-1 B'W for one sample
        // rate, where column m of B is band m's response on the grid at a 12 dB
        // reference gain, per dB. A band the DSP drops at this rate has a zero
        // column, so lambda holds it at 0 dB; grid points past Nyquist weigh nothing.
        struct Solver
        {
            static constexpr int N = DSPConstants::EQ_NUM_BANDS;
            double rate = 0.0;
            double P[N][kGrid] {};
            double B[kGrid][N] {};
            double M[N][2 * N] {};

            void build(double sampleRate)
            {
                rate = sampleRate;
                constexpr double refDb = 12.0;
                constexpr double lambda = 0.02;   // keeps the solve from trading huge opposite gains
                double w[kGrid];
                for (int j = 0; j < kGrid; ++j)
                {
                    w[j] = gridUsable(j, sampleRate) ? gridWeight(j) : 0.0;
                    for (int m = 0; m < N; ++m)
                        B[j][m] = bandUsable(m, sampleRate) && w[j] > 0.0
                                ? peakDb(gridFreq(j), DSPConstants::EQ_FREQS[m],
                                         bandQ(DSPConstants::EQ_FREQS[m], sampleRate), refDb, sampleRate) / refDb
                                : 0.0;
                }

                // [B'WB + lambda I | I], Gauss-Jordan with partial pivoting.
                for (int r = 0; r < N; ++r)
                    for (int c = 0; c < N; ++c)
                    {
                        double s = 0.0;
                        for (int j = 0; j < kGrid; ++j)
                            s += w[j] * B[j][r] * B[j][c];
                        M[r][c] = s + (r == c ? lambda : 0.0);
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
                    for (int j = 0; j < kGrid; ++j)
                    {
                        double s = 0.0;
                        for (int k = 0; k < N; ++k)
                            s += M[m][N + k] * B[j][k];
                        P[m][j] = s * w[j];
                    }
            }
        };

        // One cached solver per thread (the editor and a host restoring state may
        // fit at once), rebuilt when the rate changes. Building one is well under a
        // millisecond.
        inline const Solver& solver(double sampleRate)
        {
            thread_local Solver s;
            if (s.rate != sampleRate)
                s.build(sampleRate);
            return s;
        }

        inline double fitRate(double sampleRate) noexcept
        {
            return sampleRate > 0.0 ? sampleRate : DSPConstants::EQ_FIT_SAMPLE_RATE;
        }

        // Gains whose response best matches a target given on every grid point.
        inline Bands fitGrid(const double (&grid)[kGrid], double sampleRate)
        {
            using namespace DSPConstants;
            constexpr int N = EQ_NUM_BANDS;

            Bands gains {};
            bool flat = true;
            for (double v : grid)
                flat = flat && v == 0.0;
            if (flat)
                return gains;

            // Newton steps with a fixed Jacobian: the peaking response is close to
            // linear in its dB gain, so a handful of corrections absorbs what isn't.
            const auto& s = solver(sampleRate);
            for (int iter = 0; iter < 5; ++iter)
            {
                double residual[kGrid];
                for (int j = 0; j < kGrid; ++j)
                    residual[j] = gridUsable(j, sampleRate)
                                ? grid[j] - responseDb(gains, gridFreq(j), sampleRate) : 0.0;
                for (int m = 0; m < N; ++m)
                {
                    double step = 0.0;
                    for (int j = 0; j < kGrid; ++j)
                        step += s.P[m][j] * residual[j];
                    const double g = gains[(size_t) m] + step;
                    gains[(size_t) m] = (float) std::fmin(EQ_BAND_MAX_DB, std::fmax(EQ_BAND_MIN_DB, g));
                }
            }
            // The params step in 0.01 dB; round here so a refit of the same curve
            // writes identical values.
            for (auto& g : gains)
                g = std::round(g * 100.0f) / 100.0f;
            return gains;
        }
    }

    /** Band gains whose summed response at sampleRate follows a drawn curve, given
        as one target level (dB) per band centre and taken as a straight line in log
        frequency between centres. Targets are clamped to the band range; gains land
        in it. Bands the DSP drops at this rate stay at 0 dB. A flat target gives
        exactly flat gains, so the bank stays bypassed. sampleRate <= 0 means
        EQ_FIT_SAMPLE_RATE. Allocation-free but not cheap: call it off the audio
        thread. */
    inline Bands fit(const Bands& targetDb, double sampleRate = DSPConstants::EQ_FIT_SAMPLE_RATE)
    {
        using namespace DSPConstants;
        auto clampDb = [](float v)
        { return (double) std::fmin(EQ_BAND_MAX_DB, std::fmax(EQ_BAND_MIN_DB, v)); };

        double grid[detail::kGrid];
        for (int j = 0; j < detail::kGrid; ++j)
        {
            const int i = j / 2;
            grid[j] = (j % 2 == 0) ? clampDb(targetDb[(size_t) i])
                                   : 0.5 * (clampDb(targetDb[(size_t) i]) + clampDb(targetDb[(size_t) i + 1]));
        }
        return detail::fitGrid(grid, detail::fitRate(sampleRate));
    }

    /** Band gains reproducing a v2.3 curve (12 bands, Q 1.4) at sampleRate, for
        migrating old states onto the 31-band bank. The old response is matched at
        every centre and midpoint, not drawn as straight lines between centres, so
        an old peak that sat between two new centres keeps its height. */
    inline Bands fitLegacy(const float (&legacyGainsDb)[DSPConstants::EQ_LEGACY_NUM_BANDS],
                           double sampleRate = DSPConstants::EQ_FIT_SAMPLE_RATE)
    {
        using namespace DSPConstants;
        const double rate = detail::fitRate(sampleRate);
        double grid[detail::kGrid];
        for (int j = 0; j < detail::kGrid; ++j)
        {
            double sum = 0.0;
            for (int m = 0; m < EQ_LEGACY_NUM_BANDS; ++m)
                // The v2.3 bank dropped its bands past the same ratio.
                if (legacyGainsDb[m] != 0.0f && EQ_LEGACY_FREQS[m] < rate * EQ_MAX_FREQ_RATIO)
                    sum += peakDb(detail::gridFreq(j), EQ_LEGACY_FREQS[m], EQ_LEGACY_Q,
                                  legacyGainsDb[m], rate);
            grid[j] = std::fmin(EQ_BAND_MAX_DB, std::fmax(EQ_BAND_MIN_DB, sum));
        }
        return detail::fitGrid(grid, rate);
    }
}
