/*
 * CArtHandler.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "ArtifactUtils.h"
#include "CGeneralTextHandler.h"
#include "GameSettings.h"
#include "mapObjects/MapObjects.h"
#include "constants/StringConstants.h"

#include "mapObjectConstructors/AObjectTypeHandler.h"
#include "mapObjectConstructors/CObjectClassesHandler.h"
#include "serializer/JsonSerializeFormat.h"

// Note: list must match entries in ArtTraits.txt
#define ART_POS_LIST    \
	ART_POS(SPELLBOOK)  \
	ART_POS(MACH4)      \
	ART_POS(MACH3)      \
	ART_POS(MACH2)      \
	ART_POS(MACH1)      \
	ART_POS(MISC5)      \
	ART_POS(MISC4)      \
	ART_POS(MISC3)      \
	ART_POS(MISC2)      \
	ART_POS(MISC1)      \
	ART_POS(FEET)       \
	ART_POS(LEFT_RING)  \
	ART_POS(RIGHT_RING) \
	ART_POS(TORSO)      \
	ART_POS(LEFT_HAND)  \
	ART_POS(RIGHT_HAND) \
	ART_POS(NECK)       \
	ART_POS(SHOULDERS)  \
	ART_POS(HEAD)

VCMI_LIB_NAMESPACE_BEGIN

bool CCombinedArtifact::isCombined() const
{
	return !(constituents.empty());
}

const std::vector<CArtifact*> & CCombinedArtifact::getConstituents() const
{
	return constituents;
}

const std::vector<CArtifact*> & CCombinedArtifact::getPartOf() const
{
	return partOf;
}

bool CScrollArtifact::isScroll() const
{
	return static_cast<const CArtifact*>(this)->getId() == ArtifactID::SPELL_SCROLL;
}

bool CGrowingArtifact::isGrowing() const
{
	return !bonusesPerLevel.empty() || !thresholdBonuses.empty();
}

std::vector <std::pair<ui16, Bonus>> & CGrowingArtifact::getBonusesPerLevel()
{
	return bonusesPerLevel;
}

const std::vector <std::pair<ui16, Bonus>> & CGrowingArtifact::getBonusesPerLevel() const
{
	return bonusesPerLevel;
}

std::vector <std::pair<ui16, Bonus>> & CGrowingArtifact::getThresholdBonuses()
{
	return thresholdBonuses;
}

const std::vector <std::pair<ui16, Bonus>> & CGrowingArtifact::getThresholdBonuses() const
{
	return thresholdBonuses;
}

int32_t CArtifact::getIndex() const
{
	return id.toEnum();
}

int32_t CArtifact::getIconIndex() const
{
	return iconIndex;
}

std::string CArtifact::getJsonKey() const
{
	return modScope + ':' + identifier;
}

void CArtifact::registerIcons(const IconRegistar & cb) const
{
	cb(getIconIndex(), 0, "ARTIFACT", image);
	cb(getIconIndex(), 0, "ARTIFACTLARGE", large);
}

ArtifactID CArtifact::getId() const
{
	return id;
}

const IBonusBearer * CArtifact::getBonusBearer() const
{
	return this;
}

std::string CArtifact::getDescriptionTranslated() const
{
	return VLC->generaltexth->translate(getDescriptionTextID());
}

std::string CArtifact::getEventTranslated() const
{
	return VLC->generaltexth->translate(getEventTextID());
}

std::string CArtifact::getNameTranslated() const
{
	return VLC->generaltexth->translate(getNameTextID());
}

std::string CArtifact::getDescriptionTextID() const
{
	return TextIdentifier("artifact", modScope, identifier, "description").get();
}

std::string CArtifact::getEventTextID() const
{
	return TextIdentifier("artifact", modScope, identifier, "event").get();
}

std::string CArtifact::getNameTextID() const
{
	return TextIdentifier("artifact", modScope, identifier, "name").get();
}

uint32_t CArtifact::getPrice() const
{
	return price;
}

CreatureID CArtifact::getWarMachine() const
{
	return warMachine;
}

bool CArtifact::isBig() const
{
	return warMachine != CreatureID::NONE;
}

bool CArtifact::isTradable() const
{
	switch(id)
	{
	case ArtifactID::SPELLBOOK:
	case ArtifactID::GRAIL:
		return false;
	default:
		return !isBig();
	}
}

bool CArtifact::canBePutAt(const CArtifactSet * artSet, ArtifactPosition slot, bool assumeDestRemoved) const
{
	auto simpleArtCanBePutAt = [this](const CArtifactSet * artSet, ArtifactPosition slot, bool assumeDestRemoved) -> bool
	{
		if(ArtifactUtils::isSlotBackpack(slot))
		{
			if(isBig() || !ArtifactUtils::isBackpackFreeSlots(artSet))
				return false;
			return true;
		}

		if(!vstd::contains(possibleSlots.at(artSet->bearerType()), slot))
			return false;

		return artSet->isPositionFree(slot, assumeDestRemoved);
	};

	auto artCanBePutAt = [this, simpleArtCanBePutAt](const CArtifactSet * artSet, ArtifactPosition slot, bool assumeDestRemoved) -> bool
	{
		if(isCombined())
		{
			if(!simpleArtCanBePutAt(artSet, slot, assumeDestRemoved))
				return false;
			if(ArtifactUtils::isSlotBackpack(slot))
				return true;

			CArtifactFittingSet fittingSet(artSet->bearerType());
			fittingSet.artifactsWorn = artSet->artifactsWorn;
			if(assumeDestRemoved)
				fittingSet.removeArtifact(slot);

			for(const auto art : constituents)
			{
				auto possibleSlot = ArtifactUtils::getArtAnyPosition(&fittingSet, art->getId());
				if(ArtifactUtils::isSlotEquipment(possibleSlot))
				{
					fittingSet.setNewArtSlot(possibleSlot, nullptr, true);
				}
				else
				{
					return false;
				}
			}
			return true;
		}
		else
		{
			return simpleArtCanBePutAt(artSet, slot, assumeDestRemoved);
		}
	};

	if(slot == ArtifactPosition::TRANSITION_POS)
		return true;

	if(slot == ArtifactPosition::FIRST_AVAILABLE)
	{
		for(const auto & slot : possibleSlots.at(artSet->bearerType()))
		{
			if(artCanBePutAt(artSet, slot, assumeDestRemoved))
				return true;
		}
		return artCanBePutAt(artSet, ArtifactPosition::BACKPACK_START, assumeDestRemoved);
	}
	else if(ArtifactUtils::isSlotBackpack(slot))
	{
		return artCanBePutAt(artSet, ArtifactPosition::BACKPACK_START, assumeDestRemoved);
	}
	else
	{
		return artCanBePutAt(artSet, slot, assumeDestRemoved);
	}
}

CArtifact::CArtifact()
	: iconIndex(ArtifactID::NONE),
	price(0)
{
	setNodeType(ARTIFACT);
	possibleSlots[ArtBearer::HERO]; //we want to generate map entry even if it will be empty
	possibleSlots[ArtBearer::CREATURE]; //we want to generate map entry even if it will be empty
	possibleSlots[ArtBearer::COMMANDER];
}

//This destructor should be placed here to avoid side effects
CArtifact::~CArtifact() = default;

int CArtifact::getArtClassSerial() const
{
	if(id == ArtifactID::SPELL_SCROLL)
		return 4;
	switch(aClass)
	{
	case ART_TREASURE:
		return 0;
	case ART_MINOR:
		return 1;
	case ART_MAJOR:
		return 2;
	case ART_RELIC:
		return 3;
	case ART_SPECIAL:
		return 5;
	}

	return -1;
}

std::string CArtifact::nodeName() const
{
	return "Artifact: " + getNameTranslated();
}

void CArtifact::addNewBonus(const std::shared_ptr<Bonus>& b)
{
	b->source = BonusSource::ARTIFACT;
	b->duration = BonusDuration::PERMANENT;
	b->description = getNameTranslated();
	CBonusSystemNode::addNewBonus(b);
}

const std::map<ArtBearer::ArtBearer, std::vector<ArtifactPosition>> & CArtifact::getPossibleSlots() const
{
	return possibleSlots;
}

void CArtifact::updateFrom(const JsonNode& data)
{
	//TODO:CArtifact::updateFrom
}

void CArtifact::setImage(int32_t iconIndex, std::string image, std::string large)
{
	this->iconIndex = iconIndex;
	this->image = image;
	this->large = large;
}

CArtHandler::~CArtHandler() = default;

std::vector<JsonNode> CArtHandler::loadLegacyData()
{
	size_t dataSize = VLC->settings()->getInteger(EGameSettings::TEXTS_ARTIFACT);

	objects.resize(dataSize);
	std::vector<JsonNode> h3Data;
	h3Data.reserve(dataSize);

	#define ART_POS(x) #x ,
	const std::vector<std::string> artSlots = { ART_POS_LIST };
	#undef ART_POS

	static std::map<char, std::string> classes =
		{{'S',"SPECIAL"}, {'T',"TREASURE"},{'N',"MINOR"},{'J',"MAJOR"},{'R',"RELIC"},};

	CLegacyConfigParser parser(TextPath::builtin("DATA/ARTRAITS.TXT"));
	CLegacyConfigParser events(TextPath::builtin("DATA/ARTEVENT.TXT"));

	parser.endLine(); // header
	parser.endLine();

	for (size_t i = 0; i < dataSize; i++)
	{
		JsonNode artData;

		artData["text"]["name"].String() = parser.readString();
		artData["text"]["event"].String() = events.readString();
		artData["value"].Float() = parser.readNumber();

		for(const auto & artSlot : artSlots)
		{
			if(parser.readString() == "x")
			{
				artData["slot"].Vector().push_back(JsonNode());
				artData["slot"].Vector().back().String() = artSlot;
			}
		}
		artData["class"].String() = classes[parser.readString()[0]];
		artData["text"]["description"].String() = parser.readString();

		parser.endLine();
		events.endLine();
		h3Data.push_back(artData);
	}
	return h3Data;
}

void CArtHandler::loadObject(std::string scope, std::string name, const JsonNode & data)
{
	auto * object = loadFromJson(scope, data, name, objects.size());

	object->iconIndex = object->getIndex() + 5;

	objects.emplace_back(object);

	registerObject(scope, "artifact", name, object->id);
}

void CArtHandler::loadObject(std::string scope, std::string name, const JsonNode & data, size_t index)
{
	auto * object = loadFromJson(scope, data, name, index);

	object->iconIndex = object->getIndex();

	assert(objects[index] == nullptr); // ensure that this id was not loaded before
	objects[index] = object;

	registerObject(scope, "artifact", name, object->id);
}

const std::vector<std::string> & CArtHandler::getTypeNames() const
{
	static const std::vector<std::string> typeNames = { "artifact" };
	return typeNames;
}

CArtifact * CArtHandler::loadFromJson(const std::string & scope, const JsonNode & node, const std::string & identifier, size_t index)
{
	assert(identifier.find(':') == std::string::npos);
	assert(!scope.empty());

	CArtifact * art = new CArtifact();
	if(!node["growing"].isNull())
	{
		for(auto bonus : node["growing"]["bonusesPerLevel"].Vector())
		{
			art->bonusesPerLevel.emplace_back(static_cast<ui16>(bonus["level"].Float()), Bonus());
			JsonUtils::parseBonus(bonus["bonus"], &art->bonusesPerLevel.back().second);
		}
		for(auto bonus : node["growing"]["thresholdBonuses"].Vector())
		{
			art->thresholdBonuses.emplace_back(static_cast<ui16>(bonus["level"].Float()), Bonus());
			JsonUtils::parseBonus(bonus["bonus"], &art->thresholdBonuses.back().second);
		}
	}
	art->id = ArtifactID(index);
	art->identifier = identifier;
	art->modScope = scope;

	const JsonNode & text = node["text"];

	VLC->generaltexth->registerString(scope, art->getNameTextID(), text["name"].String());
	VLC->generaltexth->registerString(scope, art->getDescriptionTextID(), text["description"].String());
	VLC->generaltexth->registerString(scope, art->getEventTextID(), text["event"].String());

	const JsonNode & graphics = node["graphics"];
	art->image = graphics["image"].String();

	if(!graphics["large"].isNull())
		art->large = graphics["large"].String();
	else
		art->large = art->image;

	art->advMapDef = graphics["map"].String();

	art->price = static_cast<ui32>(node["value"].Float());
	art->onlyOnWaterMap = node["onlyOnWaterMap"].Bool();

	loadSlots(art, node);
	loadClass(art, node);
	loadType(art, node);
	loadComponents(art, node);

	for(const auto & b : node["bonuses"].Vector())
	{
		auto bonus = JsonUtils::parseBonus(b);
		art->addNewBonus(bonus);
	}

	const JsonNode & warMachine = node["warMachine"];
	if(warMachine.getType() == JsonNode::JsonType::DATA_STRING && !warMachine.String().empty())
	{
		VLC->identifiers()->requestIdentifier("creature", warMachine, [=](si32 id)
		{
			art->warMachine = CreatureID(id);

			//this assumes that creature object is stored before registration
			VLC->creh->objects.at(id)->warMachine = art->id;
		});
	}

	VLC->identifiers()->requestIdentifier(scope, "object", "artifact", [=](si32 index)
	{
		JsonNode conf;
		conf.setMeta(scope);

		VLC->objtypeh->loadSubObject(art->identifier, conf, Obj::ARTIFACT, art->getIndex());

		if(!art->advMapDef.empty())
		{
			JsonNode templ;
			templ["animation"].String() = art->advMapDef;
			templ.setMeta(scope);

			// add new template.
			// Necessary for objects added via mods that don't have any templates in H3
			VLC->objtypeh->getHandlerFor(Obj::ARTIFACT, art->getIndex())->addTemplate(templ);
		}
		// object does not have any templates - this is not usable object (e.g. pseudo-art like lock)
		if(VLC->objtypeh->getHandlerFor(Obj::ARTIFACT, art->getIndex())->getTemplates().empty())
			VLC->objtypeh->removeSubObject(Obj::ARTIFACT, art->getIndex());
	});

	return art;
}

int32_t ArtifactPositionBase::decode(const std::string & slotName)
{
#define ART_POS(x) { #x, ArtifactPosition::x },
	static const std::map<std::string, ArtifactPosition> artifactPositionMap = { ART_POS_LIST };
#undef ART_POS
	auto it = artifactPositionMap.find (slotName);
	if (it != artifactPositionMap.end())
		return it->second;
	else
		return PRE_FIRST;
}

void CArtHandler::addSlot(CArtifact * art, const std::string & slotID) const
{
	static const std::vector<ArtifactPosition> miscSlots =
	{
		ArtifactPosition::MISC1, ArtifactPosition::MISC2, ArtifactPosition::MISC3, ArtifactPosition::MISC4, ArtifactPosition::MISC5
	};

	static const std::vector<ArtifactPosition> ringSlots =
	{
		ArtifactPosition::RIGHT_RING, ArtifactPosition::LEFT_RING
	};

	if (slotID == "MISC")
	{
		vstd::concatenate(art->possibleSlots[ArtBearer::HERO], miscSlots);
	}
	else if (slotID == "RING")
	{
		vstd::concatenate(art->possibleSlots[ArtBearer::HERO], ringSlots);
	}
	else
	{
		auto slot = ArtifactPosition::decode(slotID);
		if (slot != ArtifactPosition::PRE_FIRST)
			art->possibleSlots[ArtBearer::HERO].push_back(slot);
	}
}

void CArtHandler::loadSlots(CArtifact * art, const JsonNode & node) const
{
	if (!node["slot"].isNull()) //we assume non-hero slots are irrelevant?
	{
		if (node["slot"].getType() == JsonNode::JsonType::DATA_STRING)
			addSlot(art, node["slot"].String());
		else
		{
			for (const JsonNode & slot : node["slot"].Vector())
				addSlot(art, slot.String());
		}
		std::sort(art->possibleSlots.at(ArtBearer::HERO).begin(), art->possibleSlots.at(ArtBearer::HERO).end());
	}
}

CArtifact::EartClass CArtHandler::stringToClass(const std::string & className)
{
	static const std::map<std::string, CArtifact::EartClass> artifactClassMap =
	{
		{"TREASURE", CArtifact::ART_TREASURE},
		{"MINOR", CArtifact::ART_MINOR},
		{"MAJOR", CArtifact::ART_MAJOR},
		{"RELIC", CArtifact::ART_RELIC},
		{"SPECIAL", CArtifact::ART_SPECIAL}
	};

	auto it = artifactClassMap.find (className);
	if (it != artifactClassMap.end())
		return it->second;

	logMod->warn("Warning! Artifact rarity %s not recognized!", className);
	return CArtifact::ART_SPECIAL;
}

void CArtHandler::loadClass(CArtifact * art, const JsonNode & node) const
{
	art->aClass = stringToClass(node["class"].String());
}

void CArtHandler::loadType(CArtifact * art, const JsonNode & node) const
{
#define ART_BEARER(x) { #x, ArtBearer::x },
	static const std::map<std::string, int> artifactBearerMap = { ART_BEARER_LIST };
#undef ART_BEARER

	for (const JsonNode & b : node["type"].Vector())
	{
		auto it = artifactBearerMap.find (b.String());
		if (it != artifactBearerMap.end())
		{
			int bearerType = it->second;
			switch (bearerType)
			{
				case ArtBearer::HERO://TODO: allow arts having several possible bearers
					break;
				case ArtBearer::COMMANDER:
					makeItCommanderArt (art); //original artifacts should have only one bearer type
					break;
				case ArtBearer::CREATURE:
					makeItCreatureArt (art);
					break;
			}
		}
		else
			logMod->warn("Warning! Artifact type %s not recognized!", b.String());
	}
}

void CArtHandler::loadComponents(CArtifact * art, const JsonNode & node)
{
	if (!node["components"].isNull())
	{
		for(const auto & component : node["components"].Vector())
		{
			VLC->identifiers()->requestIdentifier("artifact", component, [=](si32 id)
			{
				// when this code is called both combinational art as well as component are loaded
				// so it is safe to access any of them
				art->constituents.push_back(objects[id]);
				objects[id]->partOf.push_back(art);
			});
		}
	}
}

ArtifactID CArtHandler::pickRandomArtifact(CRandomGenerator & rand, int flags, std::function<bool(ArtifactID)> accepts)
{
	auto getAllowedArts = [&](std::vector<ConstTransitivePtr<CArtifact> > &out, std::vector<CArtifact*> *arts, CArtifact::EartClass flag)
	{
		if (arts->empty()) //restock available arts
			fillList(*arts, flag);

		for (auto & arts_i : *arts)
		{
			if (accepts(arts_i->id))
			{
				CArtifact *art = arts_i;
				out.emplace_back(art);
			}
		}
	};

	auto getAllowed = [&](std::vector<ConstTransitivePtr<CArtifact> > &out)
	{
		if (flags & CArtifact::ART_TREASURE)
			getAllowedArts (out, &treasures, CArtifact::ART_TREASURE);
		if (flags & CArtifact::ART_MINOR)
			getAllowedArts (out, &minors, CArtifact::ART_MINOR);
		if (flags & CArtifact::ART_MAJOR)
			getAllowedArts (out, &majors, CArtifact::ART_MAJOR);
		if (flags & CArtifact::ART_RELIC)
			getAllowedArts (out, &relics, CArtifact::ART_RELIC);
		if(out.empty()) //no artifact of specified rarity, we need to take another one
		{
			getAllowedArts (out, &treasures, CArtifact::ART_TREASURE);
			getAllowedArts (out, &minors, CArtifact::ART_MINOR);
			getAllowedArts (out, &majors, CArtifact::ART_MAJOR);
			getAllowedArts (out, &relics, CArtifact::ART_RELIC);
		}
		if(out.empty()) //no arts are available at all
		{
			out.resize (64);
			std::fill_n (out.begin(), 64, objects[2]); //Give Grail - this can't be banned (hopefully)
		}
	};

	std::vector<ConstTransitivePtr<CArtifact> > out;
	getAllowed(out);
	ArtifactID artID = (*RandomGeneratorUtil::nextItem(out, rand))->id;
	erasePickedArt(artID);
	return artID;
}

ArtifactID CArtHandler::pickRandomArtifact(CRandomGenerator & rand, std::function<bool(ArtifactID)> accepts)
{
	return pickRandomArtifact(rand, 0xff, std::move(accepts));
}

ArtifactID CArtHandler::pickRandomArtifact(CRandomGenerator & rand, int flags)
{
	return pickRandomArtifact(rand, flags, [](const ArtifactID &) { return true; });
}

void CArtHandler::makeItCreatureArt(CArtifact * a, bool onlyCreature)
{
	if (onlyCreature)
	{
		a->possibleSlots[ArtBearer::HERO].clear();
		a->possibleSlots[ArtBearer::COMMANDER].clear();
	}
	a->possibleSlots[ArtBearer::CREATURE].push_back(ArtifactPosition::CREATURE_SLOT);
}

void CArtHandler::makeItCommanderArt(CArtifact * a, bool onlyCommander)
{
	if (onlyCommander)
	{
		a->possibleSlots[ArtBearer::HERO].clear();
		a->possibleSlots[ArtBearer::CREATURE].clear();
	}
	for (int i = ArtifactPosition::COMMANDER1; i <= ArtifactPosition::COMMANDER6; ++i)
		a->possibleSlots[ArtBearer::COMMANDER].push_back(ArtifactPosition(i));
}

bool CArtHandler::legalArtifact(const ArtifactID & id)
{
	auto art = objects[id];
	//assert ( (!art->constituents) || art->constituents->size() ); //artifacts is not combined or has some components

	if(art->isCombined())
		return false; //no combo artifacts spawning

	if(art->aClass < CArtifact::ART_TREASURE || art->aClass > CArtifact::ART_RELIC)
		return false; // invalid class

	if(!art->possibleSlots[ArtBearer::HERO].empty())
		return true;

	if(!art->possibleSlots[ArtBearer::CREATURE].empty() && VLC->settings()->getBoolean(EGameSettings::MODULE_STACK_ARTIFACT))
		return true;

	if(!art->possibleSlots[ArtBearer::COMMANDER].empty() && VLC->settings()->getBoolean(EGameSettings::MODULE_COMMANDERS))
		return true;

	return false;
}

void CArtHandler::initAllowedArtifactsList(const std::vector<bool> &allowed)
{
	allowedArtifacts.clear();
	treasures.clear();
	minors.clear();
	majors.clear();
	relics.clear();

	for (ArtifactID i=ArtifactID::SPELLBOOK; i < ArtifactID(static_cast<si32>(objects.size())); i.advance(1))
	{
		if (allowed[i] && legalArtifact(ArtifactID(i)))
			allowedArtifacts.push_back(objects[i]);
			 //keep im mind that artifact can be worn by more than one type of bearer
	}
}

std::vector<bool> CArtHandler::getDefaultAllowed() const
{
	std::vector<bool> allowedArtifacts;
	allowedArtifacts.resize(127, true);
	allowedArtifacts.resize(141, false);
	allowedArtifacts.resize(size(), true);
	return allowedArtifacts;
}

void CArtHandler::erasePickedArt(const ArtifactID & id)
{
	CArtifact *art = objects[id];

	std::vector<CArtifact*> * artifactList = nullptr;
	switch(art->aClass)
	{
		case CArtifact::ART_TREASURE:
			artifactList = &treasures;
			break;
		case CArtifact::ART_MINOR:
			artifactList = &minors;
			break;
		case CArtifact::ART_MAJOR:
			artifactList = &majors;
			break;
		case CArtifact::ART_RELIC:
			artifactList = &relics;
			break;
		default:
			logMod->warn("Problem: cannot find list for artifact %s, strange class. (special?)", art->getNameTranslated());
			return;
	}

	if(artifactList->empty())
		fillList(*artifactList, art->aClass);

	auto itr = vstd::find(*artifactList, art);
	if(itr != artifactList->end())
	{
		artifactList->erase(itr);
	}
	else
		logMod->warn("Problem: cannot erase artifact %s from list, it was not present", art->getNameTranslated());
}

void CArtHandler::fillList( std::vector<CArtifact*> &listToBeFilled, CArtifact::EartClass artifactClass )
{
	assert(listToBeFilled.empty());
	for (auto & elem : allowedArtifacts)
	{
		if (elem->aClass == artifactClass)
			listToBeFilled.push_back(elem);
	}
}

void CArtHandler::afterLoadFinalization()
{
	//All artifacts have their id, so we can properly update their bonuses' source ids.
	for(auto &art : objects)
	{
		for(auto &bonus : art->getExportedBonusList())
		{
			assert(art == objects[art->id]);
			assert(bonus->source == BonusSource::ARTIFACT);
			bonus->sid = art->id;
		}
	}
	CBonusSystemNode::treeHasChanged();
}

CArtifactSet::~CArtifactSet() = default;

const CArtifactInstance * CArtifactSet::getArt(const ArtifactPosition & pos, bool excludeLocked) const
{
	if(const ArtSlotInfo * si = getSlot(pos))
	{
		if(si->artifact && (!excludeLocked || !si->locked))
			return si->artifact;
	}

	return nullptr;
}

CArtifactInstance * CArtifactSet::getArt(const ArtifactPosition & pos, bool excludeLocked)
{
	return const_cast<CArtifactInstance*>((const_cast<const CArtifactSet*>(this))->getArt(pos, excludeLocked));
}

ArtifactPosition CArtifactSet::getArtPos(const ArtifactID & aid, bool onlyWorn, bool allowLocked) const
{
	const auto result = getAllArtPositions(aid, onlyWorn, allowLocked, false);
	return result.empty() ? ArtifactPosition{ArtifactPosition::PRE_FIRST} : result[0];
}

std::vector<ArtifactPosition> CArtifactSet::getAllArtPositions(const ArtifactID & aid, bool onlyWorn, bool allowLocked, bool getAll) const
{
	std::vector<ArtifactPosition> result;
	for(const auto & slotInfo : artifactsWorn)
		if(slotInfo.second.artifact->getTypeId() == aid && (allowLocked || !slotInfo.second.locked))
			result.push_back(slotInfo.first);

	if(onlyWorn)
		return result;
	if(!getAll && !result.empty())
		return result;

	auto backpackPositions = getBackpackArtPositions(aid);
	result.insert(result.end(), backpackPositions.begin(), backpackPositions.end());
	return result;
}

std::vector<ArtifactPosition> CArtifactSet::getBackpackArtPositions(const ArtifactID & aid) const
{
	std::vector<ArtifactPosition> result;

	si32 backpackPosition = ArtifactPosition::BACKPACK_START;
	for(const auto & artInfo : artifactsInBackpack)
	{
		const auto * art = artInfo.getArt();
		if(art && art->artType->getId() == aid)
			result.emplace_back(backpackPosition);
		backpackPosition++;
	}
	return result;
}

ArtifactPosition CArtifactSet::getArtPos(const CArtifactInstance *art) const
{
	for(auto i : artifactsWorn)
		if(i.second.artifact == art)
			return i.first;

	for(int i = 0; i < artifactsInBackpack.size(); i++)
		if(artifactsInBackpack[i].artifact == art)
			return ArtifactPosition::BACKPACK_START + i;

	return ArtifactPosition::PRE_FIRST;
}

const CArtifactInstance * CArtifactSet::getArtByInstanceId(const ArtifactInstanceID & artInstId) const
{
	for(auto i : artifactsWorn)
		if(i.second.artifact->getId() == artInstId)
			return i.second.artifact;

	for(auto i : artifactsInBackpack)
		if(i.artifact->getId() == artInstId)
			return i.artifact;

	return nullptr;
}

const ArtifactPosition CArtifactSet::getSlotByInstance(const CArtifactInstance * artInst) const
{
	if(artInst)
	{
		for(const auto & slot : artInst->artType->getPossibleSlots().at(bearerType()))
			if(getArt(slot) == artInst)
				return slot;

		ArtifactPosition backpackSlot = ArtifactPosition::BACKPACK_START;
		for(auto & slotInfo : artifactsInBackpack)
		{
			if(slotInfo.getArt() == artInst)
				return backpackSlot;
			backpackSlot = ArtifactPosition(backpackSlot + 1);
		}
	}
	return ArtifactPosition::PRE_FIRST;
}

bool CArtifactSet::hasArt(const ArtifactID & aid, bool onlyWorn, bool searchBackpackAssemblies, bool allowLocked) const
{
	return getArtPosCount(aid, onlyWorn, searchBackpackAssemblies, allowLocked) > 0;
}

bool CArtifactSet::hasArtBackpack(const ArtifactID & aid) const
{
	return !getBackpackArtPositions(aid).empty();
}

unsigned CArtifactSet::getArtPosCount(const ArtifactID & aid, bool onlyWorn, bool searchBackpackAssemblies, bool allowLocked) const
{
	const auto allPositions = getAllArtPositions(aid, onlyWorn, allowLocked, true);
	if(!allPositions.empty())
		return allPositions.size();

	if(searchBackpackAssemblies && getHiddenArt(aid))
		return 1;

	return 0;
}

CArtifactSet::ArtPlacementMap CArtifactSet::putArtifact(ArtifactPosition slot, CArtifactInstance * art)
{
	ArtPlacementMap resArtPlacement;

	setNewArtSlot(slot, art, false);
	if(art->artType->isCombined() && ArtifactUtils::isSlotEquipment(slot))
	{
		const CArtifactInstance * mainPart = nullptr;
		for(const auto & part : art->getPartsInfo())
			if(vstd::contains(part.art->artType->getPossibleSlots().at(bearerType()), slot)
				&& (part.slot == ArtifactPosition::PRE_FIRST))
			{
				mainPart = part.art;
				break;
			}
		
		for(const auto & part : art->getPartsInfo())
		{
			if(part.art != mainPart)
			{
				auto partSlot = part.slot;
				if(!part.art->artType->canBePutAt(this, partSlot))
					partSlot = ArtifactUtils::getArtAnyPosition(this, part.art->getTypeId());

				assert(ArtifactUtils::isSlotEquipment(partSlot));
				setNewArtSlot(partSlot, part.art, true);
				resArtPlacement.emplace(std::make_pair(part.art, partSlot));
			}
			else
			{
				resArtPlacement.emplace(std::make_pair(part.art, part.slot));
			}
		}
	}
	return resArtPlacement;
}

void CArtifactSet::removeArtifact(ArtifactPosition slot)
{
	auto art = getArt(slot, false);
	if(art)
	{
		if(art->isCombined())
		{
			for(auto & part : art->getPartsInfo())
			{
				if(getArt(part.slot, false))
					eraseArtSlot(part.slot);
			}
		}
		eraseArtSlot(slot);
	}
}

std::pair<const CArtifactInstance *, const CArtifactInstance *> CArtifactSet::searchForConstituent(const ArtifactID & aid) const
{
	for(const auto & slot : artifactsInBackpack)
	{
		auto art = slot.artifact;
		if(art->isCombined())
		{
			for(auto & ci : art->getPartsInfo())
			{
				if(ci.art->getTypeId() == aid)
				{
					return {art, ci.art};
				}
			}
		}
	}
	return {nullptr, nullptr};
}

const CArtifactInstance * CArtifactSet::getHiddenArt(const ArtifactID & aid) const
{
	return searchForConstituent(aid).second;
}

const CArtifactInstance * CArtifactSet::getAssemblyByConstituent(const ArtifactID & aid) const
{
	return searchForConstituent(aid).first;
}

const ArtSlotInfo * CArtifactSet::getSlot(const ArtifactPosition & pos) const
{
	if(pos == ArtifactPosition::TRANSITION_POS)
	{
		// Always add to the end. Always take from the beginning.
		if(artifactsTransitionPos.empty())
			return nullptr;
		else
			return &(*artifactsTransitionPos.begin());
	}
	if(vstd::contains(artifactsWorn, pos))
		return &artifactsWorn.at(pos);
	if(pos >= ArtifactPosition::AFTER_LAST )
	{
		int backpackPos = (int)pos - ArtifactPosition::BACKPACK_START;
		if(backpackPos < 0 || backpackPos >= artifactsInBackpack.size())
			return nullptr;
		else
			return &artifactsInBackpack[backpackPos];
	}

	return nullptr;
}

bool CArtifactSet::isPositionFree(const ArtifactPosition & pos, bool onlyLockCheck) const
{
	if(const ArtSlotInfo *s = getSlot(pos))
		return (onlyLockCheck || !s->artifact) && !s->locked;

	return true; //no slot means not used
}

void CArtifactSet::setNewArtSlot(const ArtifactPosition & slot, ConstTransitivePtr<CArtifactInstance> art, bool locked)
{
	assert(!vstd::contains(artifactsWorn, slot));

	ArtSlotInfo * slotInfo;
	if(slot == ArtifactPosition::TRANSITION_POS)
	{
		// Always add to the end. Always take from the beginning.
		artifactsTransitionPos.emplace_back();
		slotInfo = &artifactsTransitionPos.back();
	}
	else if(ArtifactUtils::isSlotEquipment(slot))
	{
		slotInfo =  &artifactsWorn[slot];
	}
	else
	{
		auto position = artifactsInBackpack.begin() + slot - ArtifactPosition::BACKPACK_START;
		slotInfo = &(*artifactsInBackpack.emplace(position, ArtSlotInfo()));
	}
	slotInfo->artifact = art;
	slotInfo->locked = locked;
}

void CArtifactSet::eraseArtSlot(const ArtifactPosition & slot)
{
	if(slot == ArtifactPosition::TRANSITION_POS)
	{
		assert(!artifactsTransitionPos.empty());
		artifactsTransitionPos.erase(artifactsTransitionPos.begin());
	}
	else if(ArtifactUtils::isSlotBackpack(slot))
	{
		auto backpackSlot = ArtifactPosition(slot - ArtifactPosition::BACKPACK_START);

		assert(artifactsInBackpack.begin() + backpackSlot < artifactsInBackpack.end());
		artifactsInBackpack.erase(artifactsInBackpack.begin() + backpackSlot);
	}
	else
	{
		artifactsWorn.erase(slot);
	}
}

void CArtifactSet::artDeserializationFix(CBonusSystemNode *node)
{
	for(auto & elem : artifactsWorn)
		if(elem.second.artifact && !elem.second.locked)
			node->attachTo(*elem.second.artifact);
}

void CArtifactSet::serializeJsonArtifacts(JsonSerializeFormat & handler, const std::string & fieldName, CMap * map)
{
	//todo: creature and commander artifacts
	if(handler.saving && artifactsInBackpack.empty() && artifactsWorn.empty())
		return;

	if(!handler.saving)
	{
		assert(map);
		artifactsInBackpack.clear();
		artifactsWorn.clear();
	}

	auto s = handler.enterStruct(fieldName);

	switch(bearerType())
	{
	case ArtBearer::HERO:
		serializeJsonHero(handler, map);
		break;
	case ArtBearer::CREATURE:
		serializeJsonCreature(handler, map);
		break;
	case ArtBearer::COMMANDER:
		serializeJsonCommander(handler, map);
		break;
	default:
		assert(false);
		break;
	}
}

void CArtifactSet::serializeJsonHero(JsonSerializeFormat & handler, CMap * map)
{
	for(ArtifactPosition ap = ArtifactPosition::HEAD; ap < ArtifactPosition::AFTER_LAST; ap.advance(1))
	{
		serializeJsonSlot(handler, ap, map);
	}

	std::vector<ArtifactID> backpackTemp;

	if(handler.saving)
	{
		backpackTemp.reserve(artifactsInBackpack.size());
		for(const ArtSlotInfo & info : artifactsInBackpack)
			backpackTemp.push_back(info.artifact->getTypeId());
	}
	handler.serializeIdArray(NArtifactPosition::backpack, backpackTemp);
	if(!handler.saving)
	{
		for(const ArtifactID & artifactID : backpackTemp)
		{
			auto * artifact = ArtifactUtils::createArtifact(map, artifactID);
			auto slot = ArtifactPosition::BACKPACK_START + artifactsInBackpack.size();
			if(artifact->artType->canBePutAt(this, slot))
			{
				auto artsMap = putArtifact(slot, artifact);
				artifact->addPlacementMap(artsMap);
			}
		}
	}
}

void CArtifactSet::serializeJsonCreature(JsonSerializeFormat & handler, CMap * map)
{
	logGlobal->error("CArtifactSet::serializeJsonCreature not implemented");
}

void CArtifactSet::serializeJsonCommander(JsonSerializeFormat & handler, CMap * map)
{
	logGlobal->error("CArtifactSet::serializeJsonCommander not implemented");
}

void CArtifactSet::serializeJsonSlot(JsonSerializeFormat & handler, const ArtifactPosition & slot, CMap * map)
{
	ArtifactID artifactID;

	if(handler.saving)
	{
		const ArtSlotInfo * info = getSlot(slot);

		if(info != nullptr && !info->locked)
		{
			artifactID = info->artifact->getTypeId();
			handler.serializeId(NArtifactPosition::namesHero[slot.num], artifactID, ArtifactID::NONE);
		}
	}
	else
	{
		handler.serializeId(NArtifactPosition::namesHero[slot.num], artifactID, ArtifactID::NONE);

		if(artifactID != ArtifactID::NONE)
		{
			auto * artifact = ArtifactUtils::createArtifact(map, artifactID.toEnum());

			if(artifact->artType->canBePutAt(this, slot))
			{
				auto artsMap = putArtifact(slot, artifact);
				artifact->addPlacementMap(artsMap);
			}
			else
			{
				logGlobal->debug("Artifact can't be put at the specified location."); //TODO add more debugging information
			}
		}
	}
}

CArtifactFittingSet::CArtifactFittingSet(ArtBearer::ArtBearer Bearer):
	Bearer(Bearer)
{
}

ArtBearer::ArtBearer CArtifactFittingSet::bearerType() const
{
	return this->Bearer;
}

VCMI_LIB_NAMESPACE_END
