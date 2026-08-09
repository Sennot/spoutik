#pragma once

#include <Geode/Geode.hpp>

#include <array>

class TrainingPopup : public geode::Popup {
protected:
    bool init();
    void onClose(cocos2d::CCObject* sender) override;

    void rebuildTabs();
    void rebuildPage();
    void buildStagesPage();
    void buildStatsPage();
    void buildRoundsPage();
    void buildUnavailablePage();

    void onTab(cocos2d::CCObject* sender);
    void onStage(cocos2d::CCObject* sender);
    void onRecommended(cocos2d::CCObject* sender);
    void onLoadStage(cocos2d::CCObject* sender);
    void onResetPart(cocos2d::CCObject* sender);
    void onResetRound(cocos2d::CCObject* sender);
    void onTargetMinus(cocos2d::CCObject* sender);
    void onTargetPlus(cocos2d::CCObject* sender);
    void onTargetReset(cocos2d::CCObject* sender);
    void onSettings(cocos2d::CCObject* sender);

    cocos2d::CCNode* m_pageRoot = nullptr;
    cocos2d::CCMenu* m_tabMenu = nullptr;
    int m_activePage = 0;

public:
    static TrainingPopup* create();
};
