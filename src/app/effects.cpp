#include "effects.hpp"

#include "palette.hpp"
#include "view.hpp"

#include <gamelib/core/easing.hpp>
#include <gamelib/draw/text.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bounder::app {

namespace {

// helpers

using gamelib::core::Color;
using gamelib::core::Vec2;

constexpr float kPi = std::numbers::pi_v<float>;
const Color kDust{0.92f, 0.9f, 0.86f, 0.7f};

std::string _pointsText(const int points)
{
    return points == 0 ? std::string{"1UP"} : std::to_string(points);
}

} // namespace

Effects::Effects()
    : m_debris{500, {0.0f, 520.0f}, 0.5f}
    , m_rng{gamelib::core::randomSeed()}
{
}

void Effects::sparks(const Vec2 at, const int count, const Color color)
{
    for (int i = 0; i < count; ++i) {
        m_sparks.add({.at = at, .angle = static_cast<float>(i) / static_cast<float>(count) * 2.0f * kPi + m_rng.uniform(-0.2f, 0.2f),
                      .color = color},
                     0.4f);
    }
}

void Effects::onEvent(const game::Event& event, const game::Theme theme)
{
    using game::EventKind;
    const ThemePalette& pal = themePalette(theme);
    switch (event.kind) {
    case EventKind::Jump:
        m_puffs.burst(event.at, 2, 3.0f, {}, kDust);
        m_squash.kick(0.3f);
        break;
    case EventKind::Land:
        m_puffs.burst(event.at, 3, 3.5f, {}, kDust);
        m_squash.kick(-0.3f);
        break;
    case EventKind::Skid: m_puffs.burst(event.at, 3, 2.0f, {0.0f, -4.0f}, kDust); break;
    case EventKind::Stomp:
        sparks(event.at, 6, gamelib::core::srgb(0xFFF2A0));
        m_squash.kick(0.4f);
        break;
    case EventKind::Kick:
        sparks(event.at, 5, gamelib::core::srgb(0xFFFFFF));
        m_shake.trigger(0.25f);
        break;
    case EventKind::EnemyKnocked: sparks(event.at, 5, gamelib::core::srgb(0xFFFFFF)); break;
    case EventKind::Coin:
        if (event.value == 1) {
            m_coins.add({.at = event.at}, 0.55f); // out of a block
        } else {
            m_rings.add(event.at, 7.0f, glow(kCoinLight, 1.2f), 0.25f);
        }
        break;
    case EventKind::BrickBreak:
        for (int i = 0; i < 4; ++i) {
            const float dx = i % 2 == 0 ? -1.0f : 1.0f;
            const float up = i < 2 ? 1.0f : 0.6f;
            m_debris.emit({.position = event.at + Vec2{dx * 4.0f, (i < 2 ? -4.0f : 4.0f)},
                           .velocity = {dx * m_rng.uniform(40.0f, 70.0f), -up * m_rng.uniform(160.0f, 220.0f)},
                           .spin = dx * 8.0f, .size = 6.0f, .life = 1.1f, .color = pal[Role::Brick]});
        }
        m_shake.trigger(0.2f);
        break;
    case EventKind::BlockBump:
        if (event.value == 0) {
            m_puffs.burst(event.at + Vec2{0.0f, 16.0f}, 2, 3.0f, {0.0f, 6.0f}, kDust);
        }
        break;
    case EventKind::ItemSpawn: break;
    case EventKind::PowerUp:
        m_rings.add(event.at, 18.0f, glow(kStar, 1.3f), 0.4f);
        sparks(event.at, 8, gamelib::core::srgb(0xFFF6C0));
        break;
    case EventKind::OneUp: m_rings.add(event.at, 20.0f, glow(kOneUp, 1.3f), 0.45f); break;
    case EventKind::Hurt:
        m_flash.trigger(0.6f);
        m_shake.trigger(0.5f);
        break;
    case EventKind::Fireball: break;
    case EventKind::FireballPop:
        m_debris.burst(event.at, 5, {.speed = {30.0f, 80.0f}, .size = {1.5f, 2.5f}, .life = {0.15f, 0.3f}, .color = glow(kFire, 1.4f)});
        break;
    case EventKind::PipeIn:
    case EventKind::PipeOut: break;
    case EventKind::Checkpoint:
        m_rings.add(event.at + Vec2{0.0f, -20.0f}, 26.0f, glow(kHeroCap, 1.2f), 0.5f);
        sparks(event.at + Vec2{0.0f, -40.0f}, 10, gamelib::core::srgb(0xFFF2A0));
        break;
    case EventKind::StarCoin:
        m_rings.add(event.at, 26.0f, glow(kCoin, 1.4f), 0.5f);
        sparks(event.at, 12, gamelib::core::srgb(0xFFF2A0));
        m_flash.trigger(0.25f);
        break;
    case EventKind::GoalTouch: sparks(event.at, 10, gamelib::core::srgb(0xFFFFFF)); break;
    case EventKind::Clear: break;
    case EventKind::Death:
        m_flash.trigger(0.5f);
        m_shake.trigger(0.6f);
        m_squash.reset();
        break;
    case EventKind::HurryUp: break;
    case EventKind::BossStart: m_shake.trigger(0.4f); break;
    case EventKind::BossHit:
        sparks(event.at, 10, gamelib::core::srgb(0xFFFFFF));
        m_rings.add(event.at, 30.0f, glow(gamelib::core::srgb(0xFFFFFF), 1.2f), 0.35f);
        m_shake.trigger(0.6f);
        m_flash.trigger(0.3f);
        break;
    case EventKind::BossDefeated:
        for (int i = 0; i < 3; ++i) {
            m_debris.burst(event.at, 16, {.speed = {60.0f, 200.0f}, .size = {2.0f, 4.0f}, .life = {0.5f, 1.0f},
                                          .spin = {-8.0f, 8.0f}, .color = i == 0 ? glow(kFire, 1.3f) : glow(kStar, 1.2f)});
        }
        m_rings.add(event.at, 60.0f, glow(kStar, 1.4f), 0.7f);
        m_flash.trigger(1.0f);
        m_shake.trigger(1.0f);
        break;
    case EventKind::BossLand:
        m_puffs.burst(event.at, 6, 6.0f, {}, kDust);
        m_shake.trigger(0.45f);
        break;
    case EventKind::Points:
        m_popups.add(_pointsText(event.value), event.value == 0 ? glow(kOneUp, 1.3f) : kText, 0.8f, event.at + Vec2{0.0f, -6.0f});
        break;
    case EventKind::PlatformDrop: m_puffs.burst(event.at + Vec2{0.0f, 4.0f}, 3, 6.0f, {0.0f, 8.0f}, kDust); break;
    }
}

void Effects::firework(const Vec2 at, const Color color)
{
    m_debris.burst(at, 24, {.speed = {60.0f, 160.0f}, .size = {1.5f, 3.0f}, .life = {0.6f, 1.1f}, .color = color});
    m_rings.add(at, 30.0f, color, 0.5f);
}

void Effects::update(const float seconds)
{
    m_debris.update(seconds);
    m_puffs.update(seconds);
    m_rings.update(seconds);
    m_coins.update(seconds);
    m_popups.update(seconds);
    m_sparks.update(seconds);
    m_flash.update(seconds);
    m_shake.update(seconds);
    m_squash.update(seconds);
}

void Effects::clear()
{
    m_debris.clear();
    m_puffs.clear();
    m_rings.clear();
    m_coins.clear();
    m_popups.clear();
    m_sparks.clear();
    m_flash.clear();
    m_shake.clear();
    m_squash.reset();
}

void Effects::drawWorld(gamelib::draw::DrawList& list, const Shapes& shapes) const
{
    m_puffs.draw(list);
    m_rings.draw(list);
    for (const auto& spark : m_sparks) {
        const float t = spark.progress();
        const Vec2 dir{std::cos(spark.value.angle), std::sin(spark.value.angle)};
        const Vec2 at = spark.value.at + dir * (6.0f + 18.0f * gamelib::core::easeOutCubic(t));
        list.box(gamelib::core::Rect::fromCenter(at, {3.0f * (1.0f - t) + 0.5f, 1.2f}), spark.value.color.withAlpha(1.0f - t),
                 {.cornerRadius = 0.6f}, spark.value.angle);
    }
    for (const auto& coin : m_coins) {
        // Up out of the block and back down, then gone.
        const float t = coin.age;
        const float y = -70.0f * t + 130.0f * t * t;
        list.add({.rect = gamelib::core::Rect::fromCenter(coin.value.at + Vec2{0.0f, -10.0f + y}, {14.0f, 16.0f}),
                  .color = kCoin, .color2 = kCoinLight, .shape = shapes.item, .params = {7.0f, 0.0f, 0.0f, 0.0f}});
    }
    m_debris.draw(list, {}, {.aspect = 1.0f, .cornerRadius = 1.0f, .endScale = 0.4f});
}

void Effects::drawPopups(gamelib::draw::DrawList& list, const gamelib::core::View2D& view) const
{
    m_popups.drawWorld(list, view, {.pixelSize = 4.0f, .align = gamelib::draw::Align::Center, .shadow = true}, 60.0f);
}

void Effects::drawFlash(gamelib::draw::DrawList& list, const gamelib::core::Size canvas) const
{
    if (m_flash.active()) {
        const float a = m_flash.value();
        list.box({0.0f, 0.0f, static_cast<float>(canvas.width), static_cast<float>(canvas.height)},
                 Color{1.0f, 1.0f, 1.0f, 0.35f * a * a});
    }
}

} // namespace bounder::app
