# Third-party notices

## Blitzkrieg

Baconsistent uses Blitzkrieg as an open-source technical reference for StartPos discovery, legacy/modern percentage compatibility and pause-training UX patterns. Baconsistent adapts those ideas to a different progression model: fixed stages, configurable repetition targets, statistics and automatic rounds. No Blitzkrieg art assets are bundled.

Upstream repository: `https://github.com/ZhulinskiiDanil/blitzkrieg`

The upstream project is distributed under the MIT License. Its notice is preserved below.

MIT License

Copyright (c) 2026 Zhulynskyi Danylo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


## Death Tracker

Baconsistent v0.4.2 uses Death Tracker (`abb2k/death-tracker`) as a technical reference for detecting a noclip-suppressed death through the `PlayLayer::destroyPlayer` hook chain. Baconsistent intentionally adapts the behavior to its own training rules: noclip being enabled is not enough to invalidate a run; only an actually suppressed lethal collision before the fixed-stage endpoint blocks that attempt. No Death Tracker art assets are bundled.

Upstream repository: `https://github.com/abb2k/death-tracker`
