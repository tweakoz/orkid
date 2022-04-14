////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef  ORK_LOCALIZATION_LOCALIZATION_H_
#define  ORK_LOCALIZATION_LOCALIZATION_H_

//#include <ork/kernel/Asset.h>

#define cat2__(a,b) a ## b ## __
#define cat__(a,b)  cat2__(a,b)

//#define MAKE_TRANSLATION_KEY cat__(__translation_key_,__LINE__)
//#define ORK_LOCALIZE(x) ::ork::TranslateKey(MAKE_TRANSLATION_KEY)
//#define ORK_UNICODE(x) ::ork::TranslateKey(MAKE_TRANSLATION_KEY)
//#define ORK_LOCALIZE_KEY(x) MAKE_TRANSLATION_KEY
//#define ORK_UNICODE_KEY(x) MAKE_TRANSLATION_KEY

//#define MAKE_TRANSLATION_KEY cat__(__translation_key_,__LINE__) 
#define ORK_LOCALIZE(x) x
#define ORK_UNICODE(x) x

#include <ork/asset/Asset.h>
#include <ork/asset/AssetManager.h>
#include <ork/util/dependency/Dependent.h>
#include <ork/util/dependency/Provider.h>

#if 0

namespace ork
{

typedef int TranslationKey;
static const TranslationKey kInvalidTranslationKey = -1;

void InitLocalization(const char langcode[2]);
void LoadLocalization(const char langcode[2]);

const char *TranslateKey(TranslationKey);

class TranslationMap
	: public asset::Asset
	, private util::dependency::Dependent
{
    RttiDeclareConcrete( TranslationMap, asset::Asset );

	friend class TranslationMapLoader;
	int *mTranslationIndex;
	int mSize;
public:

	TranslationMap();

	const char *Translate(TranslationKey) const;

private:
	void Deallocate();
	/*virtual*/ void OnDependencyProvided();
	/*virtual*/ void OnDependencyRevoked();
};

class LocalizationManager : public util::dependency::Provider
{
public:
	LocalizationManager();

	static LocalizationManager &GetRef();

	PieceString GetLanguage() const;

	void LoadLocalization(const char langcode[2]);

	const char *Translate(TranslationKey key) const;

	void InitLocalization(const char langcode[2]);

	TranslationMap *GetSourceMap() const;

private:
	char mLanguageCode[2];

	char *mLanguageData;

	TranslationMap *mSourceMap;

	friend class TranslationMap;

	const char *GetString(int offset)
	{
		return mLanguageData + offset;
	}

	static LocalizationManager sManager;
};

typedef asset::AssetManager<TranslationMap> TranslationMapManager;

} // namespace ork

#endif

#endif
