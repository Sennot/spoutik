#include "../runtime/TrainingManager.hpp"
#include "../ui/PauseProgressBar.hpp"
#include "../ui/TrainingPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include <algorithm>

using namespace geode::prelude;

class $modify(BaconsistentPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        // The StartPos counter lives entirely in PauseLayer by reusing GD's
        // existing Practice Mode progress area. Refresh it while paused so
        // external StartPos switchers can change m_startPosObject live.
        baconsistent::ui::updatePauseProgressBar(this);
        this->schedule(schedule_selector(BaconsistentPauseLayer::updateBaconProgress), 0.10f);

        // Do not use absolute pause coordinates. Pause menus are a shared UI
        // surface and other mods routinely add buttons here. A compact branded
        // icon participates in Geode's existing menu layout instead.
        auto leftMenu = this->getChildByID("left-button-menu");
        if (!leftMenu) {
            log::warn("Baconsistent: left-button-menu not found; skipping pause button to avoid UI overlap");
            return;
        }

        auto logo = CCSprite::create("pause-icon.png"_spr);
        if (!logo) {
            log::error("Baconsistent: pause-icon.png failed to load");
            return;
        }

        // The source art is intentionally high resolution, but the
        // shared PauseLayer menu expects compact items. Give Geode a real 40x40
        // layout item instead of scaling a huge menu item after insertion.
        auto iconRoot = CCNode::create();
        iconRoot->setContentSize({40.f, 40.f});
        auto const logoSize = logo->getContentSize();
        if (logoSize.width > 0.f && logoSize.height > 0.f) {
            logo->setScale(std::min(36.f / logoSize.width, 36.f / logoSize.height));
        }
        logo->setPosition({20.f, 20.f});
        iconRoot->addChild(logo);

        auto button = CCMenuItemSpriteExtra::create(
            iconRoot,
            this,
            menu_selector(BaconsistentPauseLayer::onBaconsistent)
        );
        button->m_baseScale = 1.f;
        button->setID("pause-button"_spr);
        leftMenu->addChild(button);
        leftMenu->updateLayout();
    }

    void updateBaconProgress(float) {
        baconsistent::ui::updatePauseProgressBar(this);
    }

    void onBaconsistent(CCObject*) {
        if (!baconsistent::TrainingManager::get().loaded()) {
            FLAlertLayer::create(
                "Baconsistent",
                "Baconsistent supports classic percentage-based levels. Platformer mode is not supported yet.",
                "OK"
            )->show();
            return;
        }

        if (auto popup = TrainingPopup::create()) {
            popup->show();
        }
    }
};
