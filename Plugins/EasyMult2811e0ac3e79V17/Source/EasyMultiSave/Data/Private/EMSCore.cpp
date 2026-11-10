//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#include "EMSCore.h"
#include "EMSPaths.h"
#include "Serialization/BufferArchive.h"

/**
FCoreSaveArchive
**/

FArchive& FCoreSaveArchive::operator<<(UObject*& Obj)
{
	if (IsSaving())
	{
		FSoftObjectPath Path = FSoftObjectPath(Obj);
		return CleanSoftObjectPath(Path);
	}
	else if (IsLoading())
	{
		//Read the raw path from disk
		FSoftObjectPath SoftPath;

		//New version just uses the built-in function
		if (ObjectVersion >= EMS::OBJECT_INST_VERSION_2)
		{
			SoftPath.SerializePath(*this);
		}
		else
		{
			//Support legacy pointers that do not use SerializePath()
			FString LegacyPath;
			*this << LegacyPath;
			SoftPath = FSoftObjectPath(LegacyPath);
		}

		//Apply Redirects 
		SoftPath = FRedirectHelpers::ResolveLevelPath(SoftPath);

		//PIE Fixup
		FixupForPIE(SoftPath);

		//Resolve using SoftPtr logic for both Soft and Hard refs identically
		FSoftObjectPtr SoftPtr(SoftPath);
		Obj = SoftPtr.LoadSynchronous();

		if (!Obj && !SoftPath.IsNull())
		{
			UE_LOG(LogEasyMultiSave, Verbose, 
				TEXT("Hard Object Reference could not be resolved at load time, use a Soft Object Reference instead: %s"), *SoftPtr.ToString());
		}

		return *this;
	}

	return FObjectAndNameAsStringProxyArchive::operator<<(Obj);
}

FArchive& FCoreSaveArchive::operator<<(FSoftObjectPtr& Value)
{
	if (IsSaving())
	{
		FSoftObjectPath Path = Value.ToSoftObjectPath();
		return CleanSoftObjectPath(Path);
	}
	else if (IsLoading())
	{
		//Load and apply redirects
		FObjectAndNameAsStringProxyArchive::operator<<(Value);

		FSoftObjectPath SoftPath = Value.ToSoftObjectPath();
		SoftPath = FRedirectHelpers::ResolveLevelPath(SoftPath);

		//PIE Fixup
		FixupForPIE(SoftPath);

		Value = FSoftObjectPtr(SoftPath);

		return *this;
	}

	return FObjectAndNameAsStringProxyArchive::operator<<(Value);
}

FArchive& FCoreSaveArchive::operator<<(FSoftObjectPath& Value)
{
	if (IsSaving())
	{
		return CleanSoftObjectPath(Value);
	}
	else if (IsLoading())
	{
		//Load and apply redirects
		FObjectAndNameAsStringProxyArchive::operator<<(Value);
		Value = FRedirectHelpers::ResolveLevelPath(Value);

		//PIE Fixup
		FixupForPIE(Value);

		return *this;
	}

	return FObjectAndNameAsStringProxyArchive::operator<<(Value);
}

/**
Core Save Archive Helpers
**/

FArchive& FCoreSaveArchive::CleanSoftObjectPath(FSoftObjectPath& Path)
{

#if WITH_EDITOR
	//Remove any PIE prefixes
	FString PathString = Path.ToString();
	FEditorPaths::StripRefPIEPrefix(PathString);
	Path = FSoftObjectPath(PathString);
#endif

	//From EMS::OBJECT_INST_VERSION_2, we use the built-in function
	Path.SerializePath(*this);

	return *this;
}

void FCoreSaveArchive::FixupForPIE(FSoftObjectPath& Path) const
{
	/*
	Loading PIE Fixup(Editor Only)
	This is important, so we get the PIE prefix from non-resolved refs in the editor.
	But still remain compatible between editor and packaged builds.
	*/
#if WITH_EDITOR
	if (GIsPlayInEditorWorld)
	{
		Path.FixupForPIE();
	}
#endif

}
