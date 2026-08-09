#pragma once

#include "../runtime/TrainingManager.hpp"

namespace baconsistent::ui {

// Small branded transient toast shown only when an exact fixed stage has just
// been counted. This is intentionally event-driven, not a persistent gameplay HUD.
void showProgressNotification(ProgressEvent const& event);

} // namespace baconsistent::ui
