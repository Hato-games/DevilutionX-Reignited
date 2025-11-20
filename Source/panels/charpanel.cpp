#include "panels/charpanel.hpp"

#include <cstdint>
#include <string>

#include <fmt/format.h>
#include <function_ref.hpp>

#include "control.h"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "playerdat.hpp"
#include "utils/display.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/surface_to_clx.hpp"

namespace devilution {

OptionalOwnedClxSpriteList pChrButtons;
OptionalOwnedClxSpriteList pChrTabButtons;

namespace {

OptionalOwnedClxSpriteList PanelButton;
OptionalOwnedClxSpriteList PanelButtonGrime;
OptionalOwnedClxSpriteList PanelButtonDownGrime;

struct StyledText {
	UiFlags style;
	std::string text;
	int spacing = 1;
};

struct PanelEntry {
	std::string label;
	Point position;
	int length;
	int labelLength;                                               // max label's length - used for line wrapping
	std::optional<tl::function_ref<StyledText()>> statDisplayFunc; // function responsible for displaying stat
};

UiFlags GetBaseStatColor(CharacterAttribute attr)
{
	UiFlags style = UiFlags::ColorWhite;
	if (InspectPlayer->GetBaseAttributeValue(attr) == InspectPlayer->GetMaximumAttributeValue(attr))
		style = UiFlags::ColorWhitegold;
	return style;
}

UiFlags GetCurrentStatColor(CharacterAttribute attr)
{
	UiFlags style = UiFlags::ColorWhite;
	int current = InspectPlayer->GetCurrentAttributeValue(attr);
	int base = InspectPlayer->GetBaseAttributeValue(attr);
	if (current > base)
		style = UiFlags::ColorBlue;
	if (current < base)
		style = UiFlags::ColorRed;
	return style;
}

UiFlags GetValueColor(int value, bool flip = false)
{
	UiFlags style = UiFlags::ColorWhite;
	if (value > 0)
		style = (flip ? UiFlags::ColorRed : UiFlags::ColorBlue);
	if (value < 0)
		style = (flip ? UiFlags::ColorBlue : UiFlags::ColorRed);
	return style;
}

UiFlags GetValueColorCapped(int value, int maxValue, bool flip = false)
{
	UiFlags style = UiFlags::ColorWhite;
	if (maxValue != 0 && value >= maxValue)
	{
		style = UiFlags::ColorWhitegold;
	}
	else if (value > 0)
	{
		style = (flip ? UiFlags::ColorRed : UiFlags::ColorBlue);
	}
	else if (value < 0)
	{
		style = (flip ? UiFlags::ColorBlue : UiFlags::ColorRed);
	}
	return style;
}

UiFlags GetMaxManaColor()
{
	return InspectPlayer->_pMaxMana > InspectPlayer->_pMaxManaBase ? UiFlags::ColorBlue : UiFlags::ColorWhite;
}

UiFlags GetMaxHealthColor()
{
	return InspectPlayer->_pMaxHP > InspectPlayer->_pMaxHPBase ? UiFlags::ColorBlue : UiFlags::ColorWhite;
}

std::pair<int, int> GetDamage()
{
	return InspectPlayer->GetDamage();
}

StyledText GetResistInfo(int8_t resist)
{
	UiFlags style = UiFlags::ColorBlue;
	if (resist == 0)
		style = UiFlags::ColorWhite;
	else if (resist < 0)
		style = UiFlags::ColorRed;
	else if (resist >= MaxResistance)
		style = UiFlags::ColorWhitegold;

	return { style, StrCat(resist, "%") };
}

constexpr int LeftColumnLabelX = 88;
constexpr int TopRightLabelX = 211;
constexpr int RightColumnLabelX = 253;

constexpr int LeftColumnLabelWidth = 76;
constexpr int RightColumnLabelWidth = 68;

// Indices in `panelEntries`.
constexpr unsigned AttributeHeaderEntryIndices[2] = { 5, 6 };
constexpr unsigned GoldHeaderEntryIndex = 16;

constexpr int LeftColumnLabelDetailsX = 80;
constexpr int RightColumnLabelDetailsX = 253;

PanelEntry panelEntriesBasic[] = {
	{ "", { 9, 14 }, 150, 0,
	    []() { return StyledText { UiFlags::ColorWhite, InspectPlayer->_pName }; } },
	{ "", { 161, 14 }, 149, 0,
	    []() { return StyledText { UiFlags::ColorWhite, std::string(_("Lvl " + StrCat(InspectPlayer->_pLevel) + " " + PlayersData[static_cast<std::size_t>(InspectPlayer->_pClass)].className)) }; } },

	{ N_("Experience"), { 9, 35 }, 0, 99, {} },
	{ "", { 9, 55 }, 99, 91,
	    []() {
	        int spacing = ((InspectPlayer->_pExperience >= 1000000000) ? 0 : 1);
	        return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pExperience), spacing };
	    } },

	{ N_("Next level"), { 9+99, 35 }, 0, 99, {} },
	{ "", { 9 + 99, 55 }, 99, 91,
	    []() {
	        if (InspectPlayer->_pLevel == MaxCharacterLevel) {
		        return StyledText { UiFlags::ColorWhitegold, std::string(_("None")) };
	        }
	        int spacing = ((InspectPlayer->_pNextExper >= 1000000000) ? 0 : 1);
	        return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pNextExper), spacing };
	    } },

	{ N_("Gold"), { 9+99*2, 35 }, 0, 99, {} },
	{ "", { 9+99*2, 55 }, 99, 0,
	    []() { return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pGold) }; } },

	{ N_("Base"), { LeftColumnLabelX, 87 }, 0, 44, {} },
	{ N_("Now"), { 135, 87 }, 0, 44, {} },
	{ N_("Strength"), { LeftColumnLabelX, 109 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pBaseStr) }; } },
	{ "", { 135, 109 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pStrength) }; } },
	{ N_("Magic"), { LeftColumnLabelX, 109 + 28 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pBaseMag) }; } },
	{ "", { 135, 109 + 28 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pMagic) }; } },
	{ N_("Dexterity"), { LeftColumnLabelX, 109 + 28*2 }, 45, LeftColumnLabelWidth, []() { return StyledText { GetBaseStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pBaseDex) }; } },
	{ "", { 135, 109 + 28 * 2 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pDexterity) }; } },
	{ N_("Vitality"), { LeftColumnLabelX, 109 + 28 * 3 }, 45, LeftColumnLabelWidth, []() { return StyledText { GetBaseStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pBaseVit) }; } },
	{ "", { 135, 109 + 28 * 3 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pVitality) }; } },

	{ N_("Life"), { LeftColumnLabelX, 109 + 28 * 5 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetMaxHealthColor(), StrCat(InspectPlayer->_pMaxHP >> 6) }; } },
	{ "", { 135, 109 + 28 * 5 }, 45, 0,
	    []() { return StyledText { (InspectPlayer->_pHitPoints != InspectPlayer->_pMaxHP ? UiFlags::ColorRed : GetMaxHealthColor()), StrCat(InspectPlayer->_pHitPoints >> 6) }; } },
	{ N_("Mana"), { LeftColumnLabelX, 109 + 28 * 6 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetMaxManaColor(), StrCat(InspectPlayer->_pMaxMana >> 6) }; } },
	{ "", { 135, 109 + 28 * 6 }, 45, 0,
	    []() { return StyledText { (InspectPlayer->_pMana != InspectPlayer->_pMaxMana ? UiFlags::ColorRed : GetMaxManaColor()), StrCat(InspectPlayer->_pMana >> 6) }; } },

	
	{ N_("Points to distribute"), { RightColumnLabelX+7, 109 }, 50, LeftColumnLabelWidth,
	    []() {
	        InspectPlayer->_pStatPts = std::min(CalcStatDiff(*InspectPlayer), InspectPlayer->_pStatPts);
	        return StyledText { UiFlags::ColorBlue, (InspectPlayer->_pStatPts > 0 ? StrCat(InspectPlayer->_pStatPts) : "") };
	    } },

	{ N_("Elixirs consumed"), { RightColumnLabelX+7, 109 + 28 }, 50, LeftColumnLabelWidth,
	    []() {
	        int points = InspectPlayer->_pElixirsConsumed;
	        int maxPoints = InspectPlayer->GetElixirConsumeLimit();

			UiFlags color;
	        if (points > maxPoints)
			{
		        color = UiFlags::ColorOrange;
	        }
			else if (points == maxPoints)
			{
		        color = UiFlags::ColorGold;
			}
	        else if (points > 0)
			{
		        color = UiFlags::ColorBlue;
	        }
			else
			{
		        color = UiFlags::ColorWhite;
			}
	        return StyledText { color, StrCat(points) };
	    } },

	
	{ N_("Magic drop chance"), { RightColumnLabelX + 7, 109 + 28*2 }, 50, LeftColumnLabelWidth,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pDropChance), StrCat(InspectPlayer->_pDropChance, "%") }; } },


	{ "", { 7, 325-11 }, 62+8, 0,
	    []() { return StyledText { UiFlags::ColorWhite, "" }; } },

	{ "", { 251-8, 325 - 11 }, 62 + 8, 0,
	    []() { return StyledText { UiFlags::ColorWhite, "" }; } },


};

PanelEntry panelEntriesDetail[] = {
	{ "", { 9, 14 }, 150, 0,
	    []() { return StyledText { UiFlags::ColorWhite, InspectPlayer->_pName }; } },
	{ "", { 161, 14 }, 149, 0,
	    []() { return StyledText { UiFlags::ColorWhite, std::string(_("Lvl " + StrCat(InspectPlayer->_pLevel) + " " + PlayersData[static_cast<std::size_t>(InspectPlayer->_pClass)].className)) }; } },

	{ N_("Block chance"), { LeftColumnLabelDetailsX, 52 - 28 * 0 }, 57, RightColumnLabelWidth,
	    []()
		{
			int blockChanceMin = InspectPlayer->GetBlockChanceBase();
			int blockChance = InspectPlayer->_pBlockChance;

	        UiFlags color = UiFlags::ColorWhite;
	        
			if(blockChance > blockChanceMin) {
		        color = UiFlags::ColorBlue;
			}
			return StyledText { color, StrCat(InspectPlayer->_pBlockChance, "%") }; }
		},

	{ N_("Block stamina"), { LeftColumnLabelDetailsX, 52 + 28 * 1 }, 57, RightColumnLabelWidth,
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->_pBlockStaminaMax/10) }; } },


	{ N_("Armor class"), { LeftColumnLabelDetailsX, 52 + 28 * 2 }, 57, RightColumnLabelWidth,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIBonusAC), StrCat(InspectPlayer->GetArmor()) }; } },

	{ N_("Resist physical"), { LeftColumnLabelDetailsX, 52 + 28 * 4 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pPhyResist); } },
	{ N_("Resist magic"), { LeftColumnLabelDetailsX, 52 + 28 * 5 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pMagResist); } },
	{ N_("Resist fire"), { LeftColumnLabelDetailsX, 52 + 28 * 6 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pFireResist); } },
	{ N_("Resist lightning"), { LeftColumnLabelDetailsX, 52 + 28 * 7 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pLghtResist); } },


	
	{ N_("To hit"), { RightColumnLabelDetailsX, 52 + 28 * 0 }, 57, RightColumnLabelWidth,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIBonusToHit), StrCat(InspectPlayer->GetWeaponToHit(), "%") }; } },
	{ N_("Damage"), { RightColumnLabelDetailsX, 52 + 28 * 1 }, 57, RightColumnLabelWidth,
	    []() {
	        std::pair<int, int> dmg = GetDamage();
	        int spacing = ((dmg.first >= 100) ? -1 : 1);
	        return StyledText { GetValueColor(InspectPlayer->_pIBonusDam), StrCat(dmg.first, "-", dmg.second), spacing };
	    } },

	{ N_("Critical chance"), { RightColumnLabelDetailsX, 52 + 28 * 2 }, 57, RightColumnLabelWidth,
	    []() { return StyledText { GetValueColorCapped(InspectPlayer->_pCriticalChance, 100), StrCat(InspectPlayer->_pCriticalChance, "%") }; } },
	{ N_("Critical damage"), { RightColumnLabelDetailsX, 52 + 28 * 3 }, 57, RightColumnLabelWidth,
	    []()
			{
	        int critDamage = InspectPlayer->GetCriticalDamage();

	        UiFlags color;
			if (critDamage == 200)
			{
		        color = UiFlags::ColorWhite;
	        }
			else if (critDamage < 200)
			{
		        color = UiFlags::ColorRed;
			}
			else {
		        color = UiFlags::ColorBlue;
			}
	        return StyledText { color, StrCat(critDamage, "%") };
		} },

	{ N_("Spell power"), { RightColumnLabelDetailsX, 52 + 28 * 4 }, 57, RightColumnLabelWidth,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pSpellPower), StrCat(InspectPlayer->_pSpellPower, "%") }; } },

	{ N_("Max life"), { RightColumnLabelDetailsX, 52 + 28 * 6 }, 57, LeftColumnLabelWidth,
	    []() { return StyledText { GetMaxHealthColor(), StrCat(InspectPlayer->_pMaxHP >> 6) }; } },
	{ N_("Max mana"), { RightColumnLabelDetailsX, 52 + 28 * 7 }, 57, LeftColumnLabelWidth,
	    []() { return StyledText { GetMaxManaColor(), StrCat(InspectPlayer->_pMaxMana >> 6) }; } },

	
	{ "", { 7, 325 - 11 }, 62 + 8, 0,
	    []() { return StyledText { UiFlags::ColorWhite, "" }; } },

	{ "", { 251 - 8, 325 - 11 }, 62 + 8, 0,
	    []() { return StyledText { UiFlags::ColorWhite, "" }; } },
};

OptionalOwnedClxSpriteList Panel;

constexpr int PanelFieldHeight = 24;
constexpr int PanelFieldPaddingTop = 3;
constexpr int PanelFieldPaddingBottom = 3;
constexpr int PanelFieldInnerHeight = PanelFieldHeight - PanelFieldPaddingTop - PanelFieldPaddingBottom;

void DrawPanelField(const Surface &out, Point pos, int len, ClxSprite left, ClxSprite middle, ClxSprite right)
{
	RenderClxSprite(out, left, pos);
	pos.x += left.width();
	len -= left.width() + right.width();
	RenderClxSprite(out.subregion(pos.x, pos.y, len, middle.height()), middle, Point { 0, 0 });
	pos.x += len;
	RenderClxSprite(out, right, pos);
}

void DrawShadowString(const Surface &out, const PanelEntry &entry)
{
	if (entry.label.empty())
		return;

	constexpr int Spacing = 0;
	const string_view textStr = LanguageTranslate(entry.label);
	string_view text;
	std::string wrapped;
	if (entry.labelLength > 0) {
		wrapped = WordWrapString(textStr, entry.labelLength, GameFont12, Spacing);
		text = wrapped;
	} else {
		text = textStr;
	}

	UiFlags style = UiFlags::VerticalCenter;

	Point labelPosition = entry.position;

	if (entry.length == 0) {
		style |= UiFlags::AlignCenter;
	} else {
		style |= UiFlags::AlignRight;
		labelPosition += Displacement { -entry.labelLength - (IsSmallFontTall() ? 2 : 3), 0 };
	}

	DrawString(out, text, { labelPosition + Displacement { -2, 2 }, { entry.labelLength, PanelFieldHeight } }, style | UiFlags::ColorBlack, Spacing);
	DrawString(out, text, { labelPosition, { entry.labelLength, PanelFieldHeight } }, style | UiFlags::ColorWhite, Spacing);
}

void DrawStatButtons(const Surface &out)
{
	if (InspectPlayer->_pStatPts > 0 && !IsInspectingPlayer() && cpaneltab == 0) {
		if (InspectPlayer->_pBaseStr < InspectPlayer->GetMaximumAttributeValue(CharacterAttribute::Strength))
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 157 - 26 }), (*pChrButtons)[chrbtn[static_cast<size_t>(CharacterAttribute::Strength)] ? 2 : 1]);
		if (InspectPlayer->_pBaseMag < InspectPlayer->GetMaximumAttributeValue(CharacterAttribute::Magic))
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 185 - 26 }), (*pChrButtons)[chrbtn[static_cast<size_t>(CharacterAttribute::Magic)] ? 4 : 3]);
		if (InspectPlayer->_pBaseDex < InspectPlayer->GetMaximumAttributeValue(CharacterAttribute::Dexterity))
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 214 - 26 }), (*pChrButtons)[chrbtn[static_cast<size_t>(CharacterAttribute::Dexterity)] ? 6 : 5]);
		if (InspectPlayer->_pBaseVit < InspectPlayer->GetMaximumAttributeValue(CharacterAttribute::Vitality))
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 242 - 26 }), (*pChrButtons)[chrbtn[static_cast<size_t>(CharacterAttribute::Vitality)] ? 8 : 7]);
	}
}

} // namespace

void LoadCharPanel()
{
	OptionalOwnedClxSpriteList background = LoadClx("data\\charbg.clx");
	OwnedSurface out((*background)[0].width(), (*background)[0].height());
	RenderClxSprite(out, (*background)[0], { 0, 0 });
	background = std::nullopt;

	{
		PanelButton = LoadOptionalClx("data\\panel8buc.clx");
		PanelButtonGrime = LoadOptionalClx("data\\dirtybuc.clx");
		PanelButtonDownGrime = LoadOptionalClx("data\\dirtybucp.clx");

		OwnedClxSpriteList boxLeft = LoadClx("data\\boxleftend.clx");
		OwnedClxSpriteList boxMiddle = LoadClx("data\\boxmiddle.clx");
		OwnedClxSpriteList boxRight = LoadClx("data\\boxrightend.clx");

		const bool isSmallFontTall = true;


		switch (cpaneltab)
		{
			case 0:
			{

				for (auto &entry : panelEntriesBasic) {
					if (entry.statDisplayFunc) {
						DrawPanelField(out, entry.position, entry.length, boxLeft[0], boxMiddle[0], boxRight[0]);
					}
					DrawShadowString(out, entry);
				}
				
			}
				break;
		    case 1: {

			    for (auto &entry : panelEntriesDetail) {
				    if (entry.statDisplayFunc) {
					    DrawPanelField(out, entry.position, entry.length, boxLeft[0], boxMiddle[0], boxRight[0]);
				    }
				    DrawShadowString(out, entry);
			    }
		    } break;
		}


	}

	Panel = SurfaceToClx(out);
}

void FreeCharPanel()
{
	Panel = std::nullopt;
}

void DrawButtonText(const Surface &out, string_view text, Rectangle placement, UiFlags style, int spacing = 1)
{
	DrawString(out, text, { placement.position + Displacement { 0, 1 }, placement.size }, UiFlags::AlignCenter | UiFlags::KerningFitSpacing | UiFlags::ColorBlack, spacing);
	DrawString(out, text, placement, UiFlags::AlignCenter | UiFlags::KerningFitSpacing | style, spacing);
}

void RenderMainButton(const Surface &out, int buttonId, string_view text, bool selected)
{
	Point panelPosition { 0, 0 };

	Point position { 0, 0 };
	int spacing = 2;

	if(selected)
	{
		RenderClxSprite(out, (*PanelButtonGrime)[0], { 0, 0 });
		RenderClxSprite(out, (*PanelButtonDownGrime)[0], { 0, 8 });
	}
	else
	{
		RenderClxSprite(out, (*PanelButton)[0], { 0, 0 });
	}
	DrawButtonText(out, text, { position + Displacement { 0, 2 }, { out.w(), 0 } }, UiFlags::ColorButtonface, spacing);
}

void DrawChr(const Surface &out)
{
	Point pos = GetPanelPosition(UiPanels::Character, { 0, 0 });
	RenderClxSprite(out, (*Panel)[0], pos);

	switch (cpaneltab)
	{
		case 0:
		{
		
			for (auto &entry : panelEntriesBasic) {
					if (entry.statDisplayFunc) {
						StyledText tmp = (*entry.statDisplayFunc)();
						DrawString(
							out,
							tmp.text,
							{ entry.position + Displacement { pos.x, pos.y + PanelFieldPaddingTop }, { entry.length, PanelFieldInnerHeight } },
							UiFlags::AlignCenter | UiFlags::VerticalCenter | tmp.style, tmp.spacing);
					}
			}
			
		} break;
	    case 1: {

		    for (auto &entry : panelEntriesDetail) {
			        if (entry.statDisplayFunc) {
				        StyledText tmp = (*entry.statDisplayFunc)();
				        DrawString(
				            out,
				            tmp.text,
				            { entry.position + Displacement { pos.x, pos.y + PanelFieldPaddingTop }, { entry.length, PanelFieldInnerHeight } },
				            UiFlags::AlignCenter | UiFlags::VerticalCenter | tmp.style, tmp.spacing);
			        }
		    }

	    } break;
	}

	DrawStatButtons(out);



	Point posButtonBasic = GetPanelPosition(UiPanels::Character, { 7+4, 325-6 });
	Surface btnBasic = out.subregion(posButtonBasic.x, posButtonBasic.y, 62, 16);
	RenderMainButton(btnBasic, 0, _("Basic"), cpaneltab == 0);

	Point posButtonDetails = GetPanelPosition(UiPanels::Character, { 251-4, 325-6 });
	Surface btnDetails = out.subregion(posButtonDetails.x, posButtonDetails.y, 62, 16);
	RenderMainButton(btnDetails, 0, _("Detail"), cpaneltab == 1);

}


} // namespace devilution
