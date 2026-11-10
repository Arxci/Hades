//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#pragma once

#include "EMSTypes.h"
#include "CoreMinimal.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

struct FCoreArchiveContext
{

public:
	const ESaveObjectType Type;
	const uint32 Version;
	const TObjectPtr<const UObject> SaveObject;

public:

	explicit FCoreArchiveContext(const ESaveObjectType InType, const uint32 InVersion, const UObject* InSaveObject)
		: Type(InType)
		, Version(InVersion)
		, SaveObject(InSaveObject)
	{
		//SaveObject is the object actually serialized that can be used as context for anything.
	}
};


/**
Core Save Archive
The core object save archive that data is serialized into. 
**/

struct FCoreSaveArchive : public FObjectAndNameAsStringProxyArchive
{

private:
	const uint32 ObjectVersion = 0;
	const TObjectPtr<const UObject> SaveObject;

public:

	FCoreSaveArchive(FArchive& InArchive, const FCoreArchiveContext& Context)
		: FObjectAndNameAsStringProxyArchive(InArchive, true)
		, ObjectVersion(Context.Version)
		, SaveObject(Context.SaveObject)
	{
		//Consider only 'Save Game' variables.
		ArIsSaveGame = (Context.Type == ESaveObjectType::SaveGameOnly);

		//Allow to save default values.
		ArNoDelta = true;
	}

	FArchive& operator<<(UObject*& Obj) override;
	FArchive& operator<<(FSoftObjectPtr& Value) override;
	FArchive& operator<<(FSoftObjectPath& Value) override;

private:

	FArchive& CleanSoftObjectPath(FSoftObjectPath& Path);
	void FixupForPIE(FSoftObjectPath& Path) const;
};