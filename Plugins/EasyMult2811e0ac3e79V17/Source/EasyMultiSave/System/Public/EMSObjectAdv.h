//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#pragma once

#include "EMSObjectBase.h"
#include "EMSObjectAdv.generated.h"

class FBufferArchive;
class FMemoryReader;
class FMemoryWriter;
class UWorld;

UCLASS(BlueprintType, meta = (DisplayName = "Easy Multi Save Adv.", Keywords = "Save, EMS, EasyMultiSave, EasySave"))
class EASYMULTISAVE_API UEMSObjectAdv : public UEMSObjectBase
{
	GENERATED_BODY()

protected:

	//Override here, as it would call twice(EMSObject already calls the one from EMSObjectBase)
	void Initialize(FSubsystemCollectionBase& Collection) override {}
	void Deinitialize() override {}

/** Object Collection  */

public:

	/**
	Save a Collection of UObjects to a single file. 
	Provides an efficient way to store any UObject directly.Useful for Subsystems, GameInstance etc.
	 
	@param Objects     Array of serialized object states to write.
	@param bUseSlot    True to save inside the current slot directory.
	@param FileName    Custom filename identifier for the collection. Defaults to 'ObjectCollection.sav' .
	@return            True on success.
	*/
	UFUNCTION(BlueprintCallable, Category = "Easy Multi Save | Object Collection", meta = (AdvancedDisplay = "FileName"))
	bool SaveObjectCollection(const TArray<FRawObjectSaveData>& Objects, bool bUseSlot, FString FileName);

	/**
	Load a Collection of UObjects. 
	Provides an efficient way to store any UObject directly. Useful for Subsystems, GameInstance etc.
	
	@param Objects     Array of serialized object states to load on demand.
	@param bUseSlot    True to save inside the current slot directory.
	@param FileName    Custom filename identifier for the collection. Defaults to 'ObjectCollection.sav' .
	@return            True on success.
	*/
	UFUNCTION(BlueprintCallable, Category = "Easy Multi Save | Object Collection", meta = (AdvancedDisplay = "FileName"))
	bool LoadObjectCollection(const TArray<FRawObjectSaveData>& Objects, bool bUseSlot, FString FileName);

private:

	virtual bool ProcessObjectCollection(FMemoryReader& FromBinary, const FSaveObjects& Objects) const override;
	FString GetObjectCollectionPath(const bool bUseSlot, const FString& CollectionFileName) const;

	virtual FString GetCurrentSaveGameName() const override;
	virtual FString GetCurrentSaveUserName() const override;

/** Save Slot Copy */

public:

	/**
	Transfers a slot-bound Custom Save Object to another Save Slot.
	The source data remains unchanged. If SourceSaveSlot is empty, the Current Slot is used.
	A Target that resolves to the Source performs an ordinary Save of the Object instead.

	@param SaveGameClass  The class of the Custom Save Object to transfer.
	@param TargetSaveSlot The Slot that receives the transferred data.
	@param SourceSaveSlot Optional Slot to transfer from. An empty value uses the Current Slot.
	@param FileName       Optional custom file name. Allows for multiple Custom Save Objects from one Class. If empty, will use the default file name from the Class.
	@return               True on success.
	*/
	UFUNCTION(BlueprintCallable, Category = "Easy Multi Save | Save Slots", meta = (DisplayName = "Transfer Custom Object", AdvancedDisplay = "SourceSaveSlot, FileName"))
	bool TransferCustom(TSubclassOf<UEMSCustomSaveGame> SaveGameClass, const FString& TargetSaveSlot, FString SourceSaveSlot, FString FileName);

	/**
	Copies the complete data of one Save Slot or only selected Slot-bound Custom Save files.
	The Source Slot remains unchanged and the Current Slot is not switched.

	@param TargetSlot         The Slot that receives the copied data. Must not be empty or match SourceSlot.
	@param SourceSlot         Optional Slot to copy from. An empty value uses the Current Slot.
	@param CopyType           Copy the complete Slot or only the requested Slot-bound Custom files.
	@param CustomFiles        Slot-bound Custom Save file names to include. Required for Custom Files Only.
	                          A name that is empty, reserved or a duplicate destination is logged and skipped,
	                          and so is a requested file that does not exist. A requested file that exists but
	                          is corrupt fails the whole copy, in both modes.
	@param bOverwriteExisting False to abort before writing if any requested Target data already exists.
	@return                   True when the copy ran and the Target Slot Info was updated. A Full Slot copy
	                          succeeds even when every requested Custom file was skipped. Custom Files Only
	                          returns false when none of the requested files exist, since nothing was copied.
	*/
	UFUNCTION(BlueprintCallable, Category = "Easy Multi Save | Save Slots", meta = (AdvancedDisplay = "SourceSlot, CustomFiles, bOverwriteExisting", AutoCreateRefTerm = "CustomFiles"))
	bool CopySaveSlot(const FString& TargetSlot, const FString& SourceSlot, ECopySaveSlotType CopyType, const TArray<FString>& CustomFiles, bool bOverwriteExisting = true);

/** Native Cloud Saving */

public:

	/**
	Exports save data to byte array. Uses the current Save Slot by default.
	This can be used with Epic Online Services or similar cloud storage providers.

	Slot-bound file types use SourceSlot, while global Custom files ignore it.

	@param FileType    The category to export.
	@param SourceSlot  Optional Slot to export from. An empty value uses the Current Slot.
	@param CustomFiles Array of Custom file names to pack. Can be added to the Slot as well.
	@param ByteData    The resulting raw binary data.
	*/
	UFUNCTION(BlueprintCallable, Category = "Easy Multi Save | Cloud", meta = (AdvancedDisplay = "SourceSlot, CustomFiles", AutoCreateRefTerm = "CustomFiles"))
	bool ExportCloudData(ECloudFileType FileType, const FString& SourceSlot, const TArray<FString>& CustomFiles, TArray<uint8>& ByteData);

	/**
	Imports save data from byte array. Uses the current Save Slot by default.
	You can normally Load the data afterwards. Paths are resolved automatically.
	This can be used with Epic Online Services or similar cloud storage providers.

	Slot-bound file types use TargetSlot, while global Custom files ignore it.
	Importing core data into the Current Slot invalidates active runtime save state.
	Importing into another Slot is safer during live gameplay.
	
	@param ByteData   Raw data downloaded from the cloud.
	@param FileType   The category being restored. Must match the export FileType.
	@param TargetSlot Optional Slot to import into. An empty value uses the Current Slot.
	*/
	UFUNCTION(BlueprintCallable, Category = "Easy Multi Save | Cloud", meta = (AdvancedDisplay = "TargetSlot"))
	bool ImportCloudData(const TArray<uint8>& ByteData, ECloudFileType FileType, const FString& TargetSlot);

private:

	/** Reports missing, valid and invalid separately, so callers can skip absent files but never broken ones. */
	EFileValidity ReadDiskFile(const FString& FilePath, TArray<uint8>& OutData) const;
	bool WriteDiskFile(const FString& FilePath, const TArray<uint8>& InData);
	FString GetCloudFilePath(const FCloudFilePath& Path, const FString& SlotName) const;
};
