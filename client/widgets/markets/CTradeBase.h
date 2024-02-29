/*
 * CTradeBase.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "TradePanels.h"

VCMI_LIB_NAMESPACE_BEGIN

class IMarket;
class CGHeroInstance;

VCMI_LIB_NAMESPACE_END

class CButton;
class CSlider;

class CTradeBase : public CIntObject
{
public:
	struct SelectionParamOneSide
	{
		std::string text;
		int imageIndex;
	};
	using SelectionParams = std::tuple<std::optional<const SelectionParamOneSide>, std::optional<const SelectionParamOneSide>>;
	using SelectionParamsFunctor = std::function<const SelectionParams()>;

	const IMarket * market;
	const CGHeroInstance * hero;

	std::shared_ptr<TradePanelBase> bidTradePanel;
	std::shared_ptr<TradePanelBase> offerTradePanel;

	// Highlighted trade slots (nullptr if no highlight)
	std::shared_ptr<CTradeableItem> hLeft;
	std::shared_ptr<CTradeableItem> hRight;
	std::shared_ptr<CButton> deal;
	std::shared_ptr<CSlider> offerSlider;
	std::shared_ptr<CButton> maxAmount;
	std::vector<std::shared_ptr<CLabel>> labels;
	std::vector<std::shared_ptr<CTextBox>> texts;
	SelectionParamsFunctor selectionParamsCallback;
	int bidQty;
	int offerQty;

	CTradeBase(const IMarket * market, const CGHeroInstance * hero, const SelectionParamsFunctor & getParamsCallback);
	virtual void makeDeal() = 0;
	virtual void deselect();
	virtual void updateSlots();

protected:
	virtual void onSlotClickPressed(const std::shared_ptr<CTradeableItem> & newSlot, std::shared_ptr<CTradeableItem> & hCurSlot);
	virtual void updateSubtitles(EMarketMode marketMode);
	virtual void updateSelected();
	virtual CTradeBase::SelectionParams getSelectionParams() const = 0;
};

// Market subclasses
class CExperienceAltar : virtual public CTradeBase
{
public:
	std::shared_ptr<CLabel> expToLevel;
	std::shared_ptr<CLabel> expForHero;
	std::shared_ptr<CButton> sacrificeAllButton;
	const Point dealButtonPos = Point(269, 520);

	CExperienceAltar();
	virtual void sacrificeAll() = 0;
	virtual TExpType calcExpAltarForHero() = 0;
};

class CCreaturesSelling : virtual public CTradeBase
{
public:
	CCreaturesSelling();
	bool slotDeletingCheck(const std::shared_ptr<CTradeableItem> & slot);
	void updateSubtitles();
};

class CResourcesBuying : virtual public CTradeBase
{
public:
	CResourcesBuying(const CTradeableItem::ClickPressedFunctor & clickPressedCallback,
		const TradePanelBase::UpdateSlotsFunctor & updSlotsCallback);
};

class CResourcesSelling : virtual public CTradeBase
{
public:
	CResourcesSelling(const CTradeableItem::ClickPressedFunctor & clickPressedCallback);
	void updateSubtitles();
};
