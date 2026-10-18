#pragma once

#include <algorithm>

// Timing of the profile name card, the one shown while stepping through the
// profile library. A click slides the card in from the side of the arrow clicked
// (a menu pick fades it in where it is); it holds while clicks keep coming and
// fades out holdSeconds after the last one. A click while the card shows slides
// the new name across inside it instead of bringing the card in again.
//
// Pure arithmetic on the times it is handed, in seconds, so tests drive it
// without painting anything.
class ProfileCardAnimation
{
public:
    static constexpr double slideSeconds = 0.18;
    static constexpr double holdSeconds  = 1.5;
    static constexpr double fadeSeconds  = 0.3;

    // direction: +1 for the next profile (enters from the right), -1 for the
    // previous one (from the left), 0 for a menu pick (fades in place).
    void trigger(int direction, double now)
    {
        if (! isActive(now))
        {
            appearAt = now;
            enterDirection = direction;
            nameSlideAt = never;
        }
        else
        {
            // Fading out: rewind the appearance so the opacity climbs back from
            // where it is rather than jumping, without sliding the card again.
            if (now >= lastTriggerAt + holdSeconds)
            {
                appearAt = now - static_cast<double>(getCardAlpha(now)) * slideSeconds;
                enterDirection = 0;
            }
            nameSlideAt = now;
            nameDirection = direction;
        }
        lastTriggerAt = now;
        triggered = true;
    }

    // Restarts the hold while the card is not yet fading: the card stays up
    // until the profile it names has finished loading.
    void keepAlive(double now)
    {
        if (isActive(now) && now < lastTriggerAt + holdSeconds)
            lastTriggerAt = now;
    }

    bool isActive(double now) const
    {
        return triggered && now < lastTriggerAt + holdSeconds + fadeSeconds;
    }

    // True while anything is in motion; a settled card needs no repaint.
    bool isMoving(double now) const
    {
        if (! isActive(now))
            return false;
        return now < appearAt + slideSeconds
            || now < nameSlideAt + slideSeconds
            || now >= lastTriggerAt + holdSeconds;
    }

    // Card opacity, 0..1. Fades in linearly over the slide, so a rewound
    // appearance continues exactly where the fade-out left it.
    float getCardAlpha(double now) const
    {
        if (! isActive(now))
            return 0.0f;
        const double in = clamp01((now - appearAt) / slideSeconds);
        const double out = 1.0 - ease(clamp01((now - lastTriggerAt - holdSeconds) / fadeSeconds));
        return static_cast<float>(std::min(in, out));
    }

    // The card's horizontal offset as a fraction of its travel: starts at the
    // entering side (+1 right, -1 left) and eases to 0.
    float getCardOffset(double now) const
    {
        if (enterDirection == 0 || ! isActive(now))
            return 0.0f;
        return static_cast<float>(enterDirection * (1.0 - ease(clamp01((now - appearAt) / slideSeconds))));
    }

    // The name slide inside the card after a further click, 0..1 (1 = settled).
    // The incoming name moves from nameDirection to 0; the outgoing one from 0
    // to -nameDirection, fading out as it goes.
    float getNameProgress(double now) const
    {
        return static_cast<float>(ease(clamp01((now - nameSlideAt) / slideSeconds)));
    }
    int getNameDirection() const noexcept { return nameDirection; }
    bool isNameSliding(double now) const { return now < nameSlideAt + slideSeconds; }

private:
    static constexpr double never = -1.0e9;

    static double clamp01(double t) { return std::clamp(t, 0.0, 1.0); }

    // Quintic ease-in-out, the curve the fold and EXTREME animations use.
    static double ease(double t)
    {
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }

    double appearAt = never;
    double lastTriggerAt = never;
    double nameSlideAt = never;
    int enterDirection = 0;
    int nameDirection = 0;
    bool triggered = false;
};
