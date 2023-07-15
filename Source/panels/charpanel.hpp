#pragma once

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"

namespace devilution {

extern OptionalOwnedClxSpriteList pChrButtons;
extern OptionalOwnedClxSpriteList pChrTabButtons;

void DrawChr(const Surface &);
void LoadCharPanel();
void FreeCharPanel();

} // namespace devilution
