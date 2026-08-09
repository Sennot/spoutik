#include "PauseProgressBar.hpp"

#include "../runtime/TrainingManager.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace baconsistent::ui {

void updatePauseProgressBar(PauseLayer* layer) {
    if (!layer || !Mod::get()->getSettingValue<bool>("pause-startpos-progress")) {
        return;
    }

    auto& manager = TrainingManager::get();
    if (!manager.loaded() || !manager.enabled() || manager.plan().size() == 0) {
        return;
    }

    // Use the same stable node IDs Blitzkrieg targets. If another GD version or
    // mod removes any of them, fail closed and leave the pause screen untouched.
    auto* bar = layer->getChildByID("practice-progress-bar");
    auto* titleNode = layer->getChildByID("practice-mode-label");
    auto* valueNode = layer->getChildByID("practice-progress-label");

    auto* title = typeinfo_cast<CCLabelBMFont*>(titleNode);
    auto* value = typeinfo_cast<CCLabelBMFont*>(valueNode);
    if (!bar || !title || !value || !bar->getChildren() || bar->getChildrenCount() == 0) {
        return;
    }

    auto* fill = typeinfo_cast<CCSprite*>(bar->getChildren()->objectAtIndex(0));
    if (!fill) {
        return;
    }

    auto const index = std::clamp(
        manager.liveSegmentIndex(),
        0,
        static_cast<int>(manager.plan().size()) - 1
    );
    auto const stage = static_cast<std::size_t>(index);
    auto const count = manager.plan().count(stage);
    auto const target = std::max(1, manager.plan().target(stage));
    auto const left = manager.plan().remaining(stage);
    auto const ratio = std::clamp(static_cast<float>(count) / static_cast<float>(target), 0.f, 1.f);

    title->setString(fmt::format("BACONSISTENT  {}", manager.segmentRangeText(index)).c_str());

    if (manager.attemptBlockedByPractice()) {
        value->setString("PRACTICE PROTECTED - RUN IGNORED");
    }
    else if (manager.attemptBlockedByNoclip()) {
        value->setString("NOCLIP HIT - RUN IGNORED");
    }
    else if (left == 0) {
        value->setString(fmt::format("MASTERED  {}/{}", count, target).c_str());
    }
    else {
        value->setString(fmt::format("RUNS {}/{}   LEFT {}", count, target, left).c_str());
    }

    // The bar node owns the stable full width; the child is the fill sprite.
    // Always derive width from the parent so repeated live updates never
    // compound a previously shortened texture rect.
    auto const fullWidth = std::max(0.f, bar->getContentWidth());
    auto const fullHeight = std::max(0.f, bar->getContentHeight());
    auto const width = fullWidth * ratio;

    fill->setContentWidth(width);
    fill->setTextureRect({0.f, 0.f, width, fullHeight});
    if (manager.attemptBlockedByPractice() || manager.attemptBlockedByNoclip()) {
        fill->setColor(ccColor3B{255, 92, 92});
    }
    else {
        fill->setColor(left == 0
            ? ccColor3B{116, 255, 78}
            : ccColor3B{255, 177, 43});
    }
}

} // namespace baconsistent::ui
