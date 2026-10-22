#pragma once

#include <algorithm>
#include <array>

// What the XY Morph pad shows while it is being used. A press lights the puck
// at once; it stays lit while held and for holdSeconds after release, then fades
// over fadeSeconds. The trail is the last trailLength smoothed positions, one per
// pad tick, so it stretches behind a fast drag and collapses onto the puck when
// the drag stops.
//
// Pure arithmetic on the times and positions it is handed (seconds, 0..1 pad
// coordinates), so tests drive it without painting anything.
class XYPadFeedback
{
public:
    static constexpr double holdSeconds = 0.25;
    static constexpr double fadeSeconds = 0.5;
    static constexpr int    trailLength = 12;

    struct Point { float x = 0.0f, y = 0.0f; };

    void press(float x, float y)
    {
        pressed = true;
        everPressed = true;
        count = 0;
        push(x, y);
    }

    // One call per pad tick with the smoothed position, held or fading.
    void push(float x, float y)
    {
        head = (head + 1) % trailLength;
        trail[(size_t) head] = { x, y };
        count = std::min(count + 1, trailLength);
    }

    void release(double now)
    {
        if (! pressed)
            return;
        pressed = false;
        releasedAt = now;
    }

    bool isPressed() const { return pressed; }

    // 1 while held and through the hold, easing to 0 across the fade.
    float getAlpha(double now) const
    {
        if (pressed)
            return 1.0f;
        if (! everPressed)
            return 0.0f;
        const double t = (now - releasedAt - holdSeconds) / fadeSeconds;
        if (t <= 0.0)
            return 1.0f;
        if (t >= 1.0)
            return 0.0f;
        const float u = 1.0f - static_cast<float>(t);
        return u * u * (3.0f - 2.0f * u);   // smoothstep
    }

    bool isVisible(double now) const { return getAlpha(now) > 0.0f; }

    int getTrailSize() const { return count; }

    // 0 is the newest point (the puck), getTrailSize() - 1 the oldest.
    Point getTrailPoint(int age) const
    {
        const int i = ((head - age) % trailLength + trailLength) % trailLength;
        return trail[(size_t) i];
    }

private:
    std::array<Point, trailLength> trail {};
    int    head = 0;
    int    count = 0;
    bool   pressed = false;
    bool   everPressed = false;
    double releasedAt = 0.0;
};
