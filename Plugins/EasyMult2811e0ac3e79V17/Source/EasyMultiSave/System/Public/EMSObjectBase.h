//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#pragma once

#include "EMSData.h"
#include "EMSMisc.h"
#include "EMSLevel.h"
#include "EMSVersion.h"
#include "EMSPluginSettings.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EMSObjectBase.generated.h"

struct FSaveReportContext;
struct FImage;
class UTextureRenderTarget2D;
class UTexture2D;
class UBlueprintAsyncActionBase;
class UEMSInfoSaveGame;
class UEMSCustomSaveGame;
class FBufferArchive;
class FMemoryReader;
class FMemoryWriter;
class UWorld;

UCLASS(config = EmsUser, configdonotcheckdefaults, abstract, NotBlueprintType, NotBlueprintable)
class EASYMULTISAVE_API UEMSObjectBase : public UGameInstanceSubsystem
{
	GENERATED_BODY()

protected:
	UEMSObjectBase();

/** Variables  */

protected:

	const uint32 PlayerIndex = 0;

private:

	mutable FFileVersionInfo LastFileVersion;
	mutable FGameVersionInfo LastGameVersion;

private:

	UPROPERTY(config)
	FString CurrentSaveGameName;

	UPROPERTY(config)
	FString CurrentSaveUserName;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UEMSInfoSaveGame>> CachedSaveSlots;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UEMSCustomSaveGame>> CachedCustomSaves;

	UPROPERTY(Transient)
	TArray<FName> MultiSaveLevels;

/** Default Implementations  */

public:

	UWorld* GetWorld() const override;

protected:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//Implemented in EMSObject
	virtual bool UnpackActorArchive(FMemoryReader& FromBinary, const EDataLoadType LoadType) { return false; }
	virtual void ClearLoadFromMemory() {};

	//Implemented in EMSObjectAdv
	virtual bool ProcessObjectCollection(FMemoryReader& FromBinary, const FSaveObjects& Objects) const { return false; };

/** Save Object Creation  */

private:

	template <class TSaveGame>
	TSaveGame* CreateNewSaveObject(const FString& FullSavePath, const FSoftClassPath& InClassName);

/** Custom Saves  */

public:

	UEMSCustomSaveGame* GetCustomSave(const TSubclassOf<UEMSCustomSaveGame>& SaveGameClass, const FString& InSlotName,
		const FString& InFileName, bool bCacheResult = true);
	bool SaveCustom(UEMSCustomSaveGame* SaveGame);
	bool TransferCustom(UEMSCustomSaveGame* SaveGame, const FString& InTargetSlot);
	bool SaveAllCustomObjects();

	FCustomSaveInfo GetCustomSaveInfo(const UEMSCustomSaveGame* SaveGame) const;
	void DeleteCustomSave(UEMSCustomSaveGame* SaveGame);
	void ResetCustomSave(UEMSCustomSaveGame* SaveGame, const EResetCustomSaveType Type);

	bool DoesCustomSaveExist(const FString& InSlotName, const FString& InFileName) const;
	void ClearCachedCustomSave(const FString& InSlotName, const FString& InFileName);

private:

	void ClearCustomSaveByRef(const FName& CachedRefName);
	void ClearCachedCustomSaves();
	void ClearCustomSavesDesktop(const FString& SaveGameName);
	void ClearCustomSavesConsole(const FString& SaveGameName, const bool bAddFiles, TArray<FString>& OutFiles);

/** Save Slot and User Names  */

public:

	virtual FString GetCurrentSaveGameName() const;
	void SetCurrentSaveGameName(const FString& SaveGameName);
	TArray<FString> GetSortedSaveSlots() const;

	UEMSInfoSaveGame* GetSlotInfoObject(const FString& SaveGameName = FString());
	TArray<FSaveSlotInfo> GetSlotInfos(const TArray<FString>& SaveGameNames) const;
	TArray<FString> GetAllSaveGames() const;

	bool WasCurrentLevelSaved();
	void ClearMultiSaveLevels();

	bool HasSlotSaveTimePassed(const FString& SaveGameName, const double InTime) const;
	bool DoesSaveSlotExist(const FString& SaveGameName) const;
	bool DoesFullSaveGameExist(const FString& SaveGameName) const;
	void ClearCachedSaveSlot(const FString& SlotName);
	void DeleteAllSaveDataForSlot(const FString& InSaveGameName);
	void DeleteDataFromSlot(const FString& InSaveGameName, const EDeleteSlotType InData);
	void SaveSlotInfoObject(const FString& InSaveGameName, const bool bOverrideLevelData = false);

	//Shared by Custom Save transfer, Save Slot Copy and Cloud import
	bool StampSlotInfo(const FString& TargetSlot, const TCHAR* Operation);
	bool RemoveStaleBackup(const FString& TargetPath, const TCHAR* Operation) const;

	virtual FString GetCurrentSaveUserName() const;
	void SetCurrentSaveUserName(const FString& UserName);
	void DeleteAllSaveDataForUser(const FString& UserName);
	TArray<FString> GetAllSaveUsers() const;

protected:

	bool HasSaveUserName() const;
	UEMSInfoSaveGame* MakeSlotInfoObject(const FString& SaveGameName = FString());
	void ClearCachedSlots();
	virtual void ClearUserData();

/** Save and Load Archive Functions  */

protected:

	bool LoadBinaryData(const FString& FullSavePath, TArray<uint8>& OutData, EFileValidity& OutResult) const;
	bool SaveBinaryData(const FString& FullSavePath, TArray<uint8> InData) const;
	bool SaveBinaryArchive(const FString& FullSavePath, FBufferArchive& BinaryData) const;

	bool ParseArchiveData(const FLoadArchiveContext& Context, const TArray<uint8>& InData);
	bool LoadBinaryArchive(const FLoadArchiveContext& Context);

/** Save and Load Complete Objects  */

protected:

	bool SaveObject(const FString& FullSavePath, UObject* SaveGameObject) const;
	bool LoadObject(FMemoryReader& FromBinary, UObject* SaveGameObject) const;

/** Base Game-Object Serialize Functions  */

public:

	void SerializeToBinary(const UObject* Object, TArray<uint8>& OutData, const FSerializeMeta& Meta = FSerializeMeta()) const;
	void SerializeFromBinary(UObject* Object, const TArray<uint8>& InData, const FSerializeMeta& Meta = FSerializeMeta()) const;

protected:

	static void SerializeObjectWithVersion(UObject* Object, FMemoryWriter& Writer, const ESaveObjectType Type);
	void DeserializeObjectWithVersion(UObject* Object, FMemoryReader& Reader, const ESaveObjectType Type) const;

/** Debug   */
#if EMS_BUILD_DEVELOPER
protected:
	void WriteSaveReport(const FSaveReportContext& Context) const;
	void CommitSaveReportOnShutdown() const;
	void ClearReportCache(const bool bFullClear = true) const;
#else
	void WriteSaveReport(const FSaveReportContext& Context) const {}
	void CommitSaveReportOnShutdown() const {}
	void ClearReportCache(const bool bFullClear = true) const {}
#endif

/** File I/O functions   */

protected:

	EFileValidity CheckAndLoadFile(const FString& FullSavePath, TArray<uint8>& OutData, FSaveHeader& OutHeader) const;
	bool CheckAndSaveFile(const FString& FullSavePath, const TArray<uint8>& InData) const;
	bool DeleteSaveFile(const FString& FullSavePath) const;
	bool DoesFileExist(const FString& FullSavePath) const;

/** Versioning Functions  */

public:

	EIntegrityCheckResult CheckSaveGameIntegrity(const FString& FullSavePath) const;
	void ApplyLastFileVersion(FMemoryReader& MemoryReader, const bool bHandleLegacy = false) const;
	void GetGameVersion(int32& SaveFileVersion, int32& SettingsVersion) const;

/** File System and Path Names  */

public:

	FString SlotInfoSaveFile(const FString& SaveGameName = FString()) const;
	FString CustomSaveFile(const FString& InCustomSaveName, const FString& SlotName) const;
	FString ActorSaveFile(const FString& SaveGameName = FString()) const;
	FString PlayerSaveFile(const FString& SaveGameName = FString())  const;

protected:

	FString ValidateSaveName(const FString& SaveGameName) const;

	FString SaveUserDir() const;
	FString ValidatedUserName() const;
	FString UserSubDir() const;
	FString BaseSaveDir() const;
	FString BaseSavePath(const FString& FullSavePath) const;
	FString GetThumbnailFormat() const;

	FString GetFolderOrFile() const;
	FString FullSaveDir(const FString& DataType, FString SaveGameName = FString()) const;

	FString ThumbnailSaveFile(const FString& SaveGameName) const;
	FString SlotFilePath(const FString& SaveGameName = FString()) const;

	FName GetSavedSlotLevelName() const;

	bool IsFileNameReserved(const FString& SaveGameName, const bool bValidate = false) const;

/** Thumbnails  */

public:

	UTexture2D* ImportSaveThumbnail(const FString& SaveGameName) const;
	void ExportSaveThumbnail(UTextureRenderTarget2D* TextureRenderTarget, const FString& SaveGameName) const;
	void ExecuteSaveThumbnail(const FString& ThumbnailPath, FImage& ImageToCompress) const;

/** Settings Helpers  */

public:

	inline static bool IsNormalMultiLevelSave()
	{
		return FSettingHelpers::IsNormalMultiLevelSave();
	}

	inline static bool IsStreamMultiLevelSave()
	{
		return FSettingHelpers::IsStreamMultiLevelSave();
	}

	inline static bool IsFullMultiLevelSave()
	{
		return FSettingHelpers::IsFullMultiLevelSave();
	}

	inline static bool IsStackBasedMultiLevelSave()
	{
		return FSettingHelpers::IsStackBasedMultiLevelSave();
	}

	inline static bool IsContainingStreamMultiLevelSave()
	{
		return FSettingHelpers::IsContainingStreamMultiLevelSave();
	}

	inline static bool IsConsoleFileSystem()
	{
		return FSettingHelpers::IsConsoleFileSystem();
	}

	inline static bool IsMultiThreadLoading()
	{
		return FSettingHelpers::IsMultiThreadLoading();
	}

	inline static bool IsDeferredLoading()
	{
		return FSettingHelpers::IsDeferredLoading();
	}

/** Save Helpers  */

public:

	inline TArray<uint8> BytesFromString(const FString& String) const
	{
		return FSaveHelpers::BytesFromString(String);
	}

	inline FString StringFromBytes(const TArray<uint8>& Bytes) const
	{
		return FSaveHelpers::StringFromBytes(Bytes);
	}

	inline bool CompareIdentifiers(const TArray<uint8>& ArrayId, const FString& StringId) const
	{
		return FSaveHelpers::CompareIdentifiers(ArrayId, StringId);
	}

	inline bool IsAsyncTaskActiveWithPause(const ESaveGameMode Mode = ESaveGameMode::MODE_All, const bool bLog = true) const
	{
		return FAsyncSaveHelpers::IsAsyncTaskActiveWithPause(GetWorld(), Mode, bLog);
	}

	inline bool IsAsyncTaskActive(const bool bLog = true) const
	{
		return FAsyncSaveHelpers::IsAsyncTaskActive(ESaveGameMode::MODE_All, bLog);
	}

	inline bool IsPaused() const
	{
		return GetWorld() && GetWorld()->IsPaused();
	}
};
