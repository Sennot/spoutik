#include "ProgressNotification.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Notification.hpp>

#include <algorithm>

using namespace geode::prelude;

namespace baconsistent::ui {

namespace {
CCNode* makeBrandedIcon(char const* resource) {
    auto* root = CCNode::create();
    root->setContentSize({34.f, 34.f});

    if (auto* sprite = CCSprite::create(resource)) {
        auto const size = sprite->getContentSize();
        if (size.width > 0.f && size.height > 0.f) {
            sprite->setScale(std::min(30.f / size.width, 30.f / size.height));
        }
        sprite->setPosition({17.f, 17.f});
        root->addChild(sprite);
    }
    return root;
}

void show(std::string const& text, char const* iconResource, float duration) {
    if (auto* toast = Notification::create(text.c_str(), makeBrandedIcon(iconResource), duration)) {
        toast->show();
    }
}
} // namespace

void showProgressNotification(ProgressEvent const& event) {
    if (!Mod::get()->getSettingValue<bool>("success-notifications")) {
        return;
    }

    auto& manager = TrainingManager::get();
    auto const range = manager.segmentRangeText(event.segmentIndex);

    if (event.roundCompleted) {
        show(
            fmt::format("ROUND {} COMPLETE   ROUND {} STARTED", event.completedRound, event.newRound),
            "badge-round.png"_spr,
            2.8f
        );
        return;
    }

    if (event.segmentCompleted) {
        show(
            fmt::format("{} MASTERED   {}/{}", range, event.repetitions, event.target),
            "badge-complete.png"_spr,
            2.4f
        );
        return;
    }

    auto const left = std::max(0, event.target - event.repetitions);
    show(
        fmt::format("{}   {}/{}   {} LEFT", range, event.repetitions, event.target, left),
        "pause-icon.png"_spr,
        1.8f
    );
}

} // namespace baconsistent::ui
