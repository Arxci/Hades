//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Hash/xxhash.h"
#include "EMSTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEasyMultiSave, Log, All);

#define EMS_VERSION_NUMBER 185
#define EMS_ENGINE_MIN_UE55 (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
#define EMS_ENGINE_MIN_UE57 (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)

#define EMS_PLATFORM_DESKTOP (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)
#define EMS_BUILD_DEVELOPER (!UE_BUILD_SHIPPING && EMS_PLATFORM_DESKTOP)

#define TOFLAG(Enum) (static_cast<uint32>(Enum)) 

/**
Types
**/

namespace EMS
{

	/*
	Actor Tags
	*/

	static const FName HasLoadedTag(TEXT("EMS_HasLoaded"));
	static const FName SkipSaveTag(TEXT("EMS_SkipSave"));
	static const FName PersistentTag(TEXT("EMS_Persistent"));
	static const FName SkipTransformTag(TEXT("EMS_SkipTransform"));

	/*
	File Name definitions
	*/

	/*
	Used for filenames only. Playstation requires an alphanumeric character like "x"
	Set bAllowConfidentialPlatformDefines = true; in the plugin build.cs file.
	*/
	static const FString UnderscoreFile(
#if defined(PLATFORM_PS5) && PLATFORM_PS5
		TEXT("x")
#elif defined(PLATFORM_PS4) && PLATFORM_PS4
		TEXT("x")
#else
		TEXT("_")
#endif
	);
	
	//Used for internal names only
	static const FString UnderscoreInt(TEXT("_"));  
	
	static const FString Slash(TEXT("/"));

	static const FString PlayerSuffix(TEXT("Player"));
	static const FString ActorSuffix(TEXT("Level"));
	static const FString SlotSuffix(TEXT("Slot"));
	static const FString ThumbSuffix(TEXT("Thumb"));

	static const FString UserDir(TEXT("Users"));

	static const FString RawObjectTag(TEXT("@raw"));
	static const FString CustomSaveTag(TEXT("@cso@"));

	static const FString BackupTag(TEXT("bak0"));
	static const FString FallbackName(TEXT("emssave"));

	static const FString SaveGamesFolder(TEXT("SaveGames/"));
	static const FString SaveType(TEXT(".sav"));
	static const FString TempType(TEXT(".tmp"));

	static const FString ImgFormatPNG(TEXT("png"));
	static const FString ImgFormatJPG(TEXT("jpg"));

	/*
	Names and Paths
	*/

	static const FString ObjectCollection(TEXT("ObjectCollection"));

	static const FName PersistentActors(TEXT("VirtualPersistentActorLevel"));

	static constexpr TCHAR RuntimeLevelInstance[] = (TEXT("LevelStreamingDynamic"));

	static constexpr TCHAR NativeDesktopSavePath[] = TEXT("%sSaveGames/%s.sav");

	//Same as MetaClass from UEMSPluginSettings->SlotInfoSaveGameClass
	static const FSoftClassPath DefaultSlotClass(TEXT("/Script/EasyMultiSave.EMSInfoSaveGame"));

	/*
	Magic Numbers
	*/

	static constexpr int32 BigNumber = 100000;
	static constexpr double ShortDelay = 0.1;
	static constexpr uint32 SaveEventTimeout = 2000;
	static constexpr uint32 MinAsyncWaitFrames = 10; 
	static constexpr float FallbackWaitTime = 5.f;

	/*
	Internal Versioning - Legacy
	*/

	static constexpr int LEGACY_SAVEGAME_FILE_TYPE_TAG = 0x53415647; // "SAVG"

	/*
	Internal Versioning 
	*/

	static constexpr uint32 HEADER_TAG = 0x48534D45;		// "EMSH" 
	static constexpr uint32 HEADER_VERSION_1 = 1;			// Initial
	static constexpr uint32 HEADER_VERSION_2 = 2;			// Future
	static constexpr uint32 HEADER_VERSION_LATEST = HEADER_VERSION_1;

	static constexpr uint32 OBJECT_INST_TAG = 0x4F424A54;	// "OBJT"
	static constexpr uint32 OBJECT_INST_VERSION_1 = 1;		// Initial
	static constexpr uint32 OBJECT_INST_VERSION_2 = 2;		// v178+
	static constexpr uint32 OBJECT_INST_VERSION_LATEST = OBJECT_INST_VERSION_2; 

	/* Not used yet
	static constexpr uint32 ACTOR_DATA_TAG = 0x41434454;	// "ACDT" 
	static constexpr uint32 ACTOR_DATA_VERSION_1 = 1;		// Initial
	static constexpr uint32 ACTOR_DATA_VERSION_2 = 2;		// Future
	static constexpr uint32 ACTOR_DATA_VERSION_LATEST = ACTOR_DATA_VERSION_1; 

	static constexpr uint32 LEVEL_DATA_TAG = 0x4C564454;    // "LVDT" 
	static constexpr uint32 LEVEL_DATA_VERSION_1 = 1;		// Initial
	static constexpr uint32 LEVEL_DATA_VERSION_2 = 2;		// Future
	static constexpr uint32 LEVEL_DATA_VERSION_LATEST = LEVEL_DATA_VERSION_1;
	*/

	static constexpr uint32 FOOTER_TAG = 0x46534D45;		// "EMSF"

	/*
	Header Flags
	*/
	constexpr uint32 FLAG_COMPRESSED = 1 << 0;

	/*
	Helpers
	*/

	inline static bool StringEmpty(const FString& InString) { return InString.TrimStartAndEnd().IsEmpty(); }

	static FName ToCanonicalFName(const FString& InName) 
	{
		//ToLower ensures consistency, FName stores it as a case-insensitive hash
		return FName(*InName.ToLower());
	}

	inline static const uint32 GetHash(const void* Data, const uint64 Length)
	{
		if (!Data)
		{
			return 0;
		}

		return static_cast<uint32>(FXxHash64::HashBuffer(Data, Length).Hash);
	}
}

/**
Enums - Internal
**/

//Serialized values, never change order or add entries in the middle.
UENUM()
enum class EActorType : uint8
{
	AT_Runtime,
	AT_Placed,
	AT_LevelScript,
	AT_PlayerActor,
	AT_PlayerPawn,
	AT_GameObject,
	AT_Persistent,
	AT_Destroyed,
};

UENUM()
enum class EDataLoadType : uint8
{
	DATA_Level,
	DATA_Player,
	DATA_Object,
	DATA_Collection,
	DATA_PlayerCustom,
};

UENUM()
enum class ESaveErrorType : uint8
{
	ER_Player,
	ER_Level,
	ER_Object,
	ER_Collection,
};

UENUM()
enum class EPrepareType : uint8
{
	PT_Default,
	PT_FullReload,
};

UENUM()
enum class EFileValidity : uint8
{
	FILE_VALID,
	FILE_MISSING,
	FILE_INVALID,
	FILE_INCOMPATIBLE,
};

UENUM()
enum class ESaveObjectType : uint8
{
	SaveGameOnly,
	CompleteObject,
};

UENUM()
enum class ENextStepType : uint8
{
	SaveLevel,
	FinishSave,
};

UENUM()
enum class EIntegrityCheckResult : uint8
{
	Success,
	VersionMismatch,
	Invalid,
	Unknown,
};

UENUM()
enum class ESaveEventType : uint8
{
	PreSave,
	PostSave,
};

UENUM(BlueprintType)
enum class EAsyncCheckType : uint8
{
	CT_Both,
	CT_Save,
	CT_Load,
};

/**
Enums - Settings
**/

UENUM()
enum class ELoadMethod : uint8
{
	/** Useful for small amounts of Actors. Blocks the game thread during load. */
	LM_Default   UMETA(DisplayName = "Default"),

	/** Useful large amounts of runtime spawned Actors. */
	LM_Deferred  UMETA(DisplayName = "Deferred"),

	/** Useful for large amounts of placed Actors.	*/
	LM_Thread   UMETA(DisplayName = "Multi-Thread"),
};

UENUM()
enum class EFileSaveMethod : uint8
{
	/** Each slot has it's own folder. Files are compressed. */
	FM_Desktop  UMETA(DisplayName = "Desktop"),

	/** No folders. Files are compressed. Each slot has it's own files with '_SlotName' suffix.*/
	FM_Console   UMETA(DisplayName = "Console"),
};

UENUM()
enum class EMultiLevelSaveMethod : uint8
{
	/** Multi Level Saving Disabled. */
	ML_Disabled   UMETA(DisplayName = "Disabled"),

	/** For multiple Persistent Levels only. */
	ML_Normal  UMETA(DisplayName = "Basic"),

	/** For one Persistent Level with World Partition or Streaming Sub-Levels. */
	ML_Stream  UMETA(DisplayName = "Streaming"),

	/** For multiple Persistent Levels with World Partition and/or Streaming Sub-Levels. */
	ML_Full  UMETA(DisplayName = "Full"),

};

UENUM()
enum class EThumbnailImageFormat : uint8
{
	/** Use high quality, but slow png compression. */
	Png,

	/** Use lower quality, but fast jpeg compression. */
	Jpeg,
};

UENUM()
enum class EOldPackageEngine : uint8
{
	/** Unreal Engine 4. Package Version 555. */
	EN_UE40 UMETA(DisplayName = "Unreal Engine 4"),

	/** Unreal Engine 5.0 - 5.3. Package Version 1009. */
	EN_UE50 UMETA(DisplayName = "Unreal Engine 5.0+"),

	/** Unreal Engine 5.4 and newer. Package Version 1012 or higher. */
	EN_UE54 UMETA(DisplayName = "Unreal Engine 5.4+"),
};

UENUM()
enum class EAutoSaveStreamMethod : uint8
{
	/**
	Automatically saves and loads Streaming Level and World Partition data to memory.
	Changes must be manually saved to disk using 'Save Game Actors' as usual.
	*/
	Enabled,

	/** @DEPRECATED - Same as Enabled. Kept for existing configs. */
	MemoryOnly UMETA(Hidden),

	/** Disables all automatic loading and saving for Streaming Levels and World Partition. */
	Disabled,
};

UENUM()
enum class EWorldPartitionInit : uint8
{
	/** Automatically loads Level Actors on begin. */
	Default UMETA(DisplayName = "Default"),

	/** Will not automatically load the Level at the beginning. */
	Skip UMETA(DisplayName = "Skip Initial Load"),
};

UENUM()
enum class ESaveReportSetting : uint8
{
	/** Advanced Save Report is disabled. */
	Disabled UMETA(DisplayName = "Disabled"),

	/** Advanced Save Report is written when saving data. */
	OnSave UMETA(DisplayName = "On Save"),

	/** Advanced Save Report is written when loading data. */
	OnLoad UMETA(DisplayName = "On Load"),
};

/**
Enums - Blueprint
**/

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ESaveTypeFlags : uint8
{
	None = 0 UMETA(Hidden),

	/** Player Controller, Pawn and Player State. */
	SF_Player = 1 << 0 UMETA(DisplayName = "Player Actors"),

	/** Level Actors and Level Blueprints. */
	SF_Level = 1 << 1 UMETA(DisplayName = "Level Actors"),
};
ENUM_CLASS_FLAGS(ESaveTypeFlags);

namespace EMSFLAG
{
	inline static bool IsPlayer(const int32 Data) { return (Data & TOFLAG(ESaveTypeFlags::SF_Player)) != 0; }
	inline static bool IsLevel(const int32 Data) { return (Data & TOFLAG(ESaveTypeFlags::SF_Level)) != 0; }
}

UENUM(BlueprintType)
enum class ECopySaveSlotType : uint8
{
	/** Copies the complete core Save Slot and optionally selected Custom files. */
	FullSlot UMETA(DisplayName = "Complete Slot"),

	/** Copies only selected Slot-bound Custom files. Core Slot data remains unchanged. */
	CustomFiles UMETA(DisplayName = "Custom Files Only"),
};

UENUM(BlueprintType)
enum class ESaveFileCheckType : uint8
{
	/** Check Current Slot, Player and Level Files.*/
	CheckForGame UMETA(DisplayName = "Complete Slot"),

	/** Check Custom Save File without considering Slots.*/
	CheckForCustom UMETA(DisplayName = "Custom Save"),

	/** Check Custom Save File or Custom Player within the Current Save Slot.*/
	CheckForCustomSlot UMETA(DisplayName = "Custom Save in Slot"),

	/** Will Check only for the Slot Info of the Current Save Slot.*/
	CheckForSlotOnly UMETA(DisplayName = "Slot Only"),

	/** Check only the Player save file of the Current Save Slot. */
	CheckForPlayerOnly  UMETA(DisplayName = "Player Only"),

	/** Check only the Level/Actor save file of the Current Save Slot. */
	CheckForLevelOnly   UMETA(DisplayName = "Level Only"),
};

UENUM(BlueprintType)
enum class ESaveGameMode : uint8
{
	MODE_Player UMETA(DisplayName = "Player"),
	MODE_Level UMETA(DisplayName = "Level"),
	MODE_All UMETA(DisplayName = "Both"),
};

UENUM(BlueprintType)
enum class ELoadedStateMod : uint8
{
	/** The loaded state of the Actor is not changed. */
	NoModify UMETA(DisplayName = "Do Not Change"),

	/** Set the state of the Actor to unloaded. */
	Unloaded UMETA(DisplayName = "Unloaded"),

	/** Set the state of the Actor to loaded. */
	Loaded UMETA(DisplayName = "Loaded"),
};

UENUM(BlueprintType)
enum class EResetCustomSaveType : uint8
{
	/** Reset property values and clears memory data. The Custom Save will be reloaded from disk. */
	Full UMETA(DisplayName = "Full Reset"),

	/** Clear only cached/memory data while keeping the current property values intact. The Custom Save will be reloaded from disk. */
	ClearMemory UMETA(DisplayName = "Memory Only"),

	/** Reset all properties to their default values for the object in memory. */
	ResetDefault UMETA(DisplayName = "Defaults Only"),
};

UENUM(BlueprintType)
enum class EDeleteSlotType : uint8
{
	/** Deletes the Complete Slot including Custom Save Objects. */
	Full UMETA(DisplayName = "Full Slot"),

	/** Deletes only the Player save data from Slot. */
	Player UMETA(DisplayName = "Player Only"),

	/** Deletes only the Level save data from Slot. */
	Level UMETA(DisplayName = "Level Only"),
};

//Identifies a single save file for cloud byte-array conversion.
UENUM(BlueprintType)
enum class ECloudFileType : uint8
{
	/** Resolved Slot with Player, Level, and Info files. Can also include Custom files inside the Slot. The Thumbnail is not included. */
	FullSlot	UMETA(DisplayName = "Complete Slot"),

	/** Any specific save file, such as Custom Objects/Players or Object Collections. */
	Custom		UMETA(DisplayName = "Custom Save"),

	/** Any specific save file inside the resolved Slot, such as Custom Objects/Players or Object Collections.*/
	CustomSlot	UMETA(DisplayName = "Custom Save in Slot"),

	/** Not exposed to Blueprints. */
	Player		UMETA(Hidden),
	Level		UMETA(Hidden),
	SlotInfo	UMETA(Hidden),
};
