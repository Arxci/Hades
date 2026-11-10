//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#include "EMSDebug.h"
#include "EMSActors.h"
#include "EMSMisc.h"
#include "EMSData.h"
#include "EMSPaths.h"
#include "EMSPluginSettings.h"
#include "EMSCustomSaveGame.h"
#include "EMSInfoSaveGame.h"
#include "Internationalization/Regex.h"
#include "HardwareInfo.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#if EMS_BUILD_DEVELOPER

namespace EmsSaveReportCommands
{
	static FAutoConsoleCommand CvarCommit
	(
		TEXT("ems.report.commit"),
		TEXT("Writes cached Save Report data to disk"),
		FConsoleCommandDelegate::CreateStatic([]()
		{
			FSaveReport::CommitSaveReport();
		})
	);

	static FAutoConsoleCommand CvarClearCache
	(
		TEXT("ems.report.clearcache"),
		TEXT("Clears advanced Save Report cache"),
		FConsoleCommandDelegate::CreateStatic([]()
		{
			FSaveReport::ClearReportCache();
		})
	);

	static FAutoConsoleCommand CvarClearFiles
	(
		TEXT("ems.report.clearfiles"),
		TEXT("Clears advanced Save Report files on disk only"),
		FConsoleCommandDelegate::CreateStatic([]()
		{
			FSaveReport::DeleteCurrentSessionFolder();
		})
	);

	static FAutoConsoleCommand CvarClearAll
	(
		TEXT("ems.report.clearall"),
		TEXT("Clears advanced Save Report cache and files on disk"),
		FConsoleCommandDelegate::CreateStatic([]()
		{
			FSaveReport::DeleteCurrentSessionFolder();
			FSaveReport::ClearReportCache();
		})
	);
}

/**
Advanced Save Data Report - Configuration and Constants
**/

static FString GetSaveReportFileType()
{
	return TEXT(".txt");
}

static FString GetSaveReportFolder()
{
	return TEXT("Ems");
}

static FString GetSaveReportCollection()
{
	return TEXT("Collection_");
}

static FString GetSessionFolder()
{
	//Static const means we create this once per session here
	static const FString SessionFolder = TEXT("Session_") + FDateTime::Now().ToString(TEXT("%Y.%m.%d_%H.%M.%S"));
	return SessionFolder;
}

static ESaveReportSetting GetSaveReportSetting()
{
	return UEMSPluginSettings::Get()->SaveReport;
}

static bool VerifyDebugSetting(const FSaveReportContextBase& Context)
{
	switch (GetSaveReportSetting())
	{
	case ESaveReportSetting::Disabled:
		return false;
	case ESaveReportSetting::OnSave:
		if (Context.bIsLoading) return false;
		break;
	case ESaveReportSetting::OnLoad:
		if (!Context.bIsLoading) return false;
		break;
	default:
		break;
	}
	return true;
}

static const FString GetOperationTypeName(const FSaveReportContextBase& Context)
{
	static const FString LoadOp = TEXT("Load");
	static const FString SaveOp = TEXT("Save");
	const ESaveReportSetting Setting = GetSaveReportSetting();

	if (Setting == ESaveReportSetting::OnLoad) return LoadOp;
	if (Setting == ESaveReportSetting::OnSave) return SaveOp;

	return Context.bIsLoading ? LoadOp : SaveOp;
}

/**
Advanced Save Data Report - Files and Path Resolution
**/

static FString GetDebugFileName(const FSaveReportContext& Context)
{
	switch (Context.DataType)
	{
	//Complete Objects like Slot and Custom-Save
	case ESaveObjectType::CompleteObject:
	{
		//Same class with different file name will write to their own files as SaveGameName is the file name
		if (const UEMSCustomSaveGame* CustomSave = Cast<UEMSCustomSaveGame>(Context.TargetObject))
		{
			return CustomSave->SaveGameName;
		}

		if (Context.TargetObject && Context.TargetObject->IsA<UEMSInfoSaveGame>())
		{
			return EMS::SlotSuffix;
		}

		return FString();
	}
	//Actors, Components, Raw Objects
	case ESaveObjectType::SaveGameOnly:
	{
		//Check if Object or its Outer is an Actor
		const AActor* TargetActor = Cast<AActor>(Context.TargetObject);
		if (!TargetActor && Context.TargetObject)
		{
			TargetActor = Context.TargetObject->GetTypedOuter<AActor>();
		}

		//Level and Player Actors including Components and Raw Objects as well
		if (TargetActor)
		{
			//Player
			const EActorType ActorType = FActorHelpers::GetActorType(TargetActor);
			if (ActorType == EActorType::AT_PlayerPawn || ActorType == EActorType::AT_PlayerActor)
			{
				return EMS::PlayerSuffix;
			}

			//Level
			return EMS::ActorSuffix;
		}

		//If not, we have object collections or unknown objects
		if (Context.IsCollection())
		{
			return GetSaveReportCollection() + Context.CollectionName;
		}

		return FString();
	}
	default:
		return FString();
	}
}

static FString GetDebugDirectoryPath(const FSaveReportContext& Context)
{
	bool bUseSlot = true;

	//Determine the operation type folder always based on live settings
	const FString TypeFolder = GetOperationTypeName(Context);
	FString BaseDir = FPaths::ProjectLogDir() / GetSaveReportFolder() / GetSessionFolder() / TypeFolder;

	//User
	if (!Context.UserName.IsEmpty())
	{
		BaseDir = BaseDir / Context.UserName;
	}

	//Custom save might be outside of slots
	if (const UEMSCustomSaveGame* CustomSave = Cast<UEMSCustomSaveGame>(Context.TargetObject))
	{
		bUseSlot = CustomSave->bUseSaveSlot;
	}

	//Object Collections are in or out of slots
	if (Context.IsCollection() && !Context.IsCollectionInSlot())
	{
		bUseSlot = false;
	}

	//Its never empty, though
	if (bUseSlot && !Context.SlotName.IsEmpty())
	{
		return BaseDir / Context.SlotName;
	}

	return BaseDir;
}

/**
Advanced Save Data Report - Text Building Parts
**/

static FString GetObjectBlockHeader()
{
	//The standard divider separating individual block subsections or properties
	static const FString Str = TEXT("==================================================\n");
	return Str;
}

static FString GetObjectBlockSplitSignature()
{
	//The end marker appended to every object block - used as a visual separator in the output file
	static const FString Str = TEXT("==================================================\n\n\n\n");
	return Str;
}

static FString GetObjectBlockSpacer()
{
	static const FString Str = TEXT("--------------------------------------------------\n");
	return Str;
}

static FString GetSessionBlockOpen()
{
	static const FString Str = TEXT("=======================================================================\n");
	return Str;
}

static FString GetSessionBlockSeparator()
{
	static const FString Str = TEXT("-----------------------------------------------------------------------\n");
	return Str;
}

static FString GetSessionBlockEnd()
{
	static const FString Str = TEXT("=======================================================================\n\n\n");
	return Str;
}

/**
Advanced Save Data Report - String Formatting 
**/

static FString GetRawTransformString(const FTransform& Transform)
{
	const FRotator Rot = Transform.Rotator();
	const FVector Pos = Transform.GetLocation();
	const FVector Scale = Transform.GetScale3D();

	//Formats Transform into macro format
	return FString::Printf(TEXT("(Rotation=(Pitch=%f,Yaw=%f,Roll=%f),Translation=(X=%f,Y=%f,Z=%f),Scale3D=(X=%f,Y=%f,Z=%f))"),
		Rot.Pitch, Rot.Yaw, Rot.Roll,
		Pos.X, Pos.Y, Pos.Z,
		Scale.X, Scale.Y, Scale.Z);
}

static FString StripDynamicSubObjectSuffixes(const FString& InFormatted)
{
	//Fast pre-check, the pattern requires "_<digits>_<32 hex chars>".
	if (!InFormatted.Contains(EMS::UnderscoreInt, ESearchCase::CaseSensitive))
	{
		return InFormatted;
	}

	static const FRegexPattern SuffixPattern(TEXT("_[0-9]+_[A-F0-9]{32}"));
	FRegexMatcher Matcher(SuffixPattern, InFormatted);

	FString Clean;
	Clean.Reserve(InFormatted.Len());
	int32 LastEnd = 0;

	while (Matcher.FindNext())
	{
		Clean += InFormatted.Mid(LastEnd, Matcher.GetMatchBeginning() - LastEnd);
		LastEnd = Matcher.GetMatchEnding();
	}
	Clean += InFormatted.Mid(LastEnd);

	return Clean;
}

static FString FormatStructString(const FString& RawValue)
{
	if (RawValue.IsEmpty())
	{
		return TEXT("()");
	}

	FString Formatted;
	Formatted.Reserve(RawValue.Len() * 2);

	int32 IndentLevel = 0;
	bool bInQuotes = false;

	enum class EGroupKind : uint8
	{
		// (X=0, Y=0) ; stays inline
		Compact,

		// ((K=..,V=..),(..))  ; unnamed entries
		ArrayOuter,

		// (Field=Value, ...)  ;  named fields, multi-line
		Expanded
	};

	struct FGroupInfo
	{
		EGroupKind Kind;
	};

	TArray<FGroupInfo> GroupStack;
	GroupStack.Reserve(16);

	//Checks if the group contains no nested parentheses (safe for inline)
	auto IsCompactGroup = [&RawValue](int32 StartIndex) -> bool
	{
		int32 Depth = 1;
		bool bQ = false;
		for (int32 j = StartIndex + 1; j < RawValue.Len(); ++j)
		{
			const TCHAR c = RawValue[j];
			if (c == '\\' && j + 1 < RawValue.Len()) { ++j; continue; }
			if (c == '"') { bQ = !bQ; continue; }
			if (bQ) continue;
			if (c == '(') return false;
			if (c == ')') { if (--Depth == 0) break; }
		}
		return true;
	};

	//Checks if the group is an outer array wrapper rather than a named struct
	auto IsArrayOuter = [&RawValue](int32 StartIndex) -> bool
	{
		for (int32 j = StartIndex + 1; j < RawValue.Len(); ++j)
		{
			const TCHAR c = RawValue[j];
			if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
			return c == '(';
		}
		return false;
	};

	//Cleans up trailing whitespaces from the formatted stream
	auto TrimTrailing = [&]()
	{
		int32 Len = Formatted.Len();
		while (Len > 0)
		{
			const TCHAR c = Formatted[Len - 1];
			if (c == ' ' || c == '\t' || c == '\n') --Len;
			else break;
		}
		Formatted.RemoveAt(Len, Formatted.Len() - Len);
	};

	//Appends spaces based on current hierarchy depth
	auto AppendIndent = [&]()
	{
		for (int32 t = 0; t < IndentLevel; ++t)
		{
			Formatted.Append(TEXT("    "));
		}
	};

	const int32 RawLen = RawValue.Len();
	for (int32 i = 0; i < RawLen; ++i)
	{
		const TCHAR Char = RawValue[i];

		//Handles literal quoted string sequences
		if (Char == '"')
		{
			bInQuotes = !bInQuotes;
			Formatted.AppendChar(Char);
			continue;
		}
		if (bInQuotes)
		{
			if (Char == '\\' && i + 1 < RawLen)
			{
				Formatted.AppendChar(Char);
				Formatted.AppendChar(RawValue[++i]);
			}
			else
			{
				Formatted.AppendChar(Char);
			}
			continue;
		}

		//Discards all raw incoming whitespaces
		if (Char == ' ' || Char == '\t' || Char == '\n' || Char == '\r')
			continue;

		//Processes opening parenthesis structural hierarchy
		if (Char == '(')
		{
			const bool bCompact = IsCompactGroup(i);

			EGroupKind Kind;
			if (bCompact)
				Kind = EGroupKind::Compact;
			else if (IsArrayOuter(i))
				Kind = EGroupKind::ArrayOuter;
			else
				Kind = EGroupKind::Expanded;

			GroupStack.Add({ Kind });

			if (Kind == EGroupKind::Compact)
			{
				Formatted.AppendChar('(');
			}
			else
			{
				TrimTrailing();
				if (Formatted.Len() > 0)
				{
					Formatted.AppendChar('\n');
					AppendIndent();
				}
				Formatted.AppendChar('(');
				Formatted.AppendChar('\n');
				IndentLevel++;
				AppendIndent();
			}
			continue;
		}

		//Processes closing parenthesis and decreases indentation depth
		if (Char == ')')
		{
			if (GroupStack.Num() > 0)
			{
				const FGroupInfo Info = GroupStack.Pop();
				if (Info.Kind != EGroupKind::Compact)
				{
					TrimTrailing();
					IndentLevel = FMath::Max(0, IndentLevel - 1);
					Formatted.AppendChar('\n');
					AppendIndent();
				}
			}

			Formatted.AppendChar(')');
			continue;
		}

		//Formats assignments with clean spacing
		if (Char == '=')
		{
			Formatted.Append(TEXT(" = "));
			while (i + 1 < RawLen && RawValue[i + 1] == ' ') ++i;
			continue;
		}

		//Appends delimiter commas and determines line splits
		if (Char == ',')
		{
			Formatted.AppendChar(',');

			const bool bCompact = GroupStack.Num() > 0 && GroupStack.Last().Kind == EGroupKind::Compact;
			if (bCompact)
			{
				Formatted.AppendChar(' ');
			}
			else
			{
				Formatted.AppendChar('\n');
				AppendIndent();
			}
			continue;
		}

		//Appends any regular standard literal characters
		Formatted.AppendChar(Char);
	}

	//Strips engine internal dynamic subobject suffix GUIDs
	Formatted = StripDynamicSubObjectSuffixes(MoveTemp(Formatted));

	return Formatted;
}

/**
Advanced Save Data Report - Reflection 
**/

static TArray<FSaveReportData> InspectObjectFields(const FSaveReportContext& Context)
{
	TArray<FSaveReportData> PropertyViews;

	const UObject* Object = Context.TargetObject;
	if (!IsValid(Object))
	{
		return PropertyViews;
	}

	//Check if only Properties with 'SaveGame' are considered, false for complete objects (Slot, Custom)
	const bool bOnlySaveGame = Context.DataType == ESaveObjectType::SaveGameOnly;

	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		//Keep this non-const
		FProperty* Prop = *It;
		if (!Prop || Prop->HasAnyPropertyFlags(CPF_Transient) || Prop->GetOwnerClass() == UObject::StaticClass())
		{
			continue;
		}

		//Only show properties flagged with SaveGame
		const bool bHasSaveFlag = Prop->HasAnyPropertyFlags(CPF_SaveGame);
		if (bOnlySaveGame && !bHasSaveFlag)
		{
			continue;
		}

		//Default Save Game, lets skip that
		static const FName CanBeDamagedName(TEXT("bCanBeDamaged"));
		if (Prop->GetOwnerClass() == AActor::StaticClass() && Prop->GetFName() == CanBeDamagedName)
		{
			continue;
		}

		FSaveReportData Data;
		Data.PropertyName = Prop->GetName();
		Data.PropertyType = Prop->GetClass()->GetName();
		Data.bIsSaveGameFlagged = bHasSaveFlag;

		Data.bIsStructType = Prop->IsA<FStructProperty>()
			|| Prop->IsA<FArrayProperty>()
			|| Prop->IsA<FMapProperty>()
			|| Prop->IsA<FSetProperty>();

		//Requires mutable for property export
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Object);

		/*
		ExportTextItem_Direct takes a non-const UObject for the outer cast.
		The function only reads the object for export context, never modifies it.
		*/
		UObject* ObjectRef = const_cast<UObject*>(Object);

		//Reads the property and copies it onto the current value
		Prop->ExportTextItem_Direct(Data.CurrentValue, ValuePtr, nullptr, ObjectRef, PPF_None);

		//Inspection != Serialize, you will get UEDPIE_ prefixes for references here
		#if WITH_EDITOR
			FEditorPaths::StripRefPIEPrefix(Data.CurrentValue);
		#endif

		PropertyViews.Add(Data);
	}

	return PropertyViews;
}

/**
Advanced Save Data Report - Layout
**/

static FString BuildTransformBlock(const UObject* Object)
{
	//Early out
	if (const AActor* Actor = Cast<AActor>(Object))
	{
		const EActorType ActorType = FActorHelpers::GetActorType(Actor);
		if (FActorHelpers::IsSkipTransform(Actor)
			|| !FActorHelpers::CanProcessActorTransform(Actor)
			|| FActorHelpers::IsLevelScript(ActorType))
		{
			return TEXT("");
		}
	}

	auto Build = [](const TCHAR* Label, const FTransform& Transform) -> FString
	{
		FString Formatted = FormatStructString(GetRawTransformString(Transform));
		return FString::Printf(TEXT("[%s] =\n%s\n"), Label, *Formatted);
	};

	if (const AActor* Actor = Cast<AActor>(Object))
	{
		return Build(TEXT("WorldTransform"), Actor->GetActorTransform());
	}

	if (const USceneComponent* Comp = Cast<USceneComponent>(Object))
	{
		return Build(TEXT("RelativeTransform"), Comp->GetRelativeTransform());
	}

	return TEXT("");
}

static FString BuildPropertyEntryLine(const FSaveReportData& Prop)
{
	if (Prop.bIsStructType)
	{
		const FString Formatted = FormatStructString(Prop.CurrentValue);
		const bool bMultiLine = Formatted.Contains(TEXT("\n"));
		if (bMultiLine)
		{
			return FString::Printf(TEXT("[%s] %s =\n%s\n"), *Prop.PropertyType, *Prop.PropertyName, *Formatted);
		}

		return FString::Printf(TEXT("[%s] %s = %s\n"), *Prop.PropertyType, *Prop.PropertyName, *Formatted);
	}

	return FString::Printf(TEXT("[%s] %s = %s\n"), *Prop.PropertyType, *Prop.PropertyName, *Prop.CurrentValue);
}

static FString BuildObjectDebugHeader(const FSaveReportContext& Context, const int32 PropertyCount)
{
	const UObject* Object = Context.TargetObject;
	if (!Object)
	{
		return TEXT("None");
	}

	FString ObjectName = Object->GetName();
	FString PathName = Object->GetPathName();
	FString ClassName = Object->GetClass()->GetPathName();
	FString EditorLabel = FString();

#if WITH_EDITOR
	FEditorPaths::StripRefPIEPrefix(PathName);
	FEditorPaths::StripRefPIEPrefix(ClassName);
#endif

	//Use helper for actor name if applicable
	if (const AActor* Actor = Cast<AActor>(Object))
	{
		ObjectName = FActorHelpers::GetFullActorName(Actor);
	}

	//Walk the full outer chain from innermost to outermost (immediate outer first)
	FString OuterChain;
	for (const UObject* Next = Object->GetOuter(); Next; Next = Next->GetOuter())
	{
		FString OuterName = Cast<AActor>(Next)
			? FActorHelpers::GetFullActorName(Cast<AActor>(Next))
			: Next->GetName();

	#if WITH_EDITOR
		FEditorPaths::StripRefPIEPrefix(OuterName);
	#endif
		//Append each outer to the end, separating with  < 
		OuterChain = OuterChain.IsEmpty() ? OuterName : OuterChain + TEXT(" < ") + OuterName;
	}

	if (OuterChain.IsEmpty())
	{
		OuterChain = TEXT("None");
	}

	FString Header;
	FString TypeSpecificLines;
	const int32 LabelWidth = -12;

	FString TypeLabel = TEXT("OBJECT:");
	const AActor* Actor = Cast<AActor>(Object);
	const UActorComponent* Comp = Cast<UActorComponent>(Object);

	if (Actor)
	{
		TypeLabel = TEXT("ACTOR:");
		const EActorType ActorType = FActorHelpers::GetActorType(Actor);

		if (FActorHelpers::IsLevelScript(ActorType))
		{
			TypeLabel = TEXT("LEVEL BP:");
		}

		//Runtime spawned
		if (FActorHelpers::IsRuntime(ActorType))
		{
			TypeSpecificLines += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("SPAWNED:"), TEXT("Runtime"));
		}

		//Net info
		const UEnum* NetRoleEnum = StaticEnum<ENetRole>();
		const FString NetRole = NetRoleEnum
			? NetRoleEnum->GetNameStringByValue((int64)Actor->GetLocalRole())
			: FString::FromInt((int64)Actor->GetLocalRole());
		TypeSpecificLines += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("NET ROLE:"), *NetRole);
		TypeSpecificLines += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("AUTHORITY:"), Actor->HasAuthority() ? TEXT("Yes") : TEXT("No"));
		TypeSpecificLines += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("REPLICATES:"), Actor->GetIsReplicated() ? TEXT("Yes") : TEXT("No"));
		TypeSpecificLines += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("HIDDEN:"), Actor->IsHidden() ? TEXT("Yes") : TEXT("No"));
		TypeSpecificLines += FString::Printf(TEXT("%*s %d\n"), LabelWidth, TEXT("COMPONENTS:"), Actor->GetComponents().Num());

		//Owner
		if (Actor->GetOwner())
		{
			TypeSpecificLines += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("OWNER:"), *Actor->GetOwner()->GetName());
		}

		//Tags
		if (Actor->Tags.Num() > 0)
		{
			TArray<FString> TagStrings;
			for (const FName& Tag : Actor->Tags)
				TagStrings.Add(Tag.ToString());
			TypeSpecificLines += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("TAGS:"), *FString::Join(TagStrings, TEXT(", ")));
		}

	#if WITH_EDITOR
		EditorLabel = Actor->GetActorLabel();
	#endif

	}
	else if (Comp)
	{
		TypeLabel = TEXT("COMPONENT:");
		TypeSpecificLines += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("ACTIVE:"),
			Comp->IsActive() ? TEXT("Yes") : TEXT("No"));
		TypeSpecificLines += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("REGISTERED:"),
			Comp->IsRegistered() ? TEXT("Yes") : TEXT("No"));
	}

	//Common header structure
	Header += GetObjectBlockHeader();

#if WITH_EDITOR
	if (Actor && !EditorLabel.IsEmpty())
	{
		Header += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("LABEL:"), *EditorLabel);
	}
#endif

	Header += FString::Printf(TEXT("%*s %s\n"), LabelWidth, *TypeLabel, *ObjectName);
	Header += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("PATH:"), *PathName);
	Header += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("CLASS:"), *ClassName);
	Header += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("OUTER:"), *OuterChain);
	Header += FString::Printf(TEXT("%*s %d\n"), LabelWidth, TEXT("PROPERTIES:"), PropertyCount);

	//Warnings
	if (Object->HasAnyFlags(RF_ClassDefaultObject))
	{
		Header += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("!! WARNING:"), TEXT("THIS IS A CDO"));
	}

	if (Object->HasAnyFlags(RF_BeginDestroyed))
	{
		Header += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("!! WARNING:"), TEXT("OBJECT IS BEING DESTROYED"));
	}

	//Type-specific lines
	Header += TypeSpecificLines;

	Header += GetObjectBlockSpacer();
	return Header;
}

static FString BuildFileSessionHeader(const FReportFileCache& FileCache, const FString& FileName) 
{
	//We only need base context without the TargetObject pointer
	const FSaveReportContextBase& BaseContext = FileCache.BaseContext;

	//Built fresh on every CommitSaveReport call - timestamp and frame counter reflect the actual flush moment
	FString HeaderOutput;
	HeaderOutput += GetSessionBlockOpen();

	const FString MapName = FileCache.bHasValidWorld ? FileCache.CachedMapName : TEXT("Unknown_Map");
	const FString ProjectVer = FApp::GetProjectName();
	const FString AppVersion = FApp::GetBuildVersion();
	const FString OpName = FileCache.OperationName;
	const int32 LabelWidth = -16;

	HeaderOutput += FString::Printf(TEXT("Advanced Save Data Report\n"));
	HeaderOutput += GetSessionBlockSeparator();

	HeaderOutput += FString::Printf(TEXT("%*s %s Operation\n"), LabelWidth, TEXT("SYSTEM STATUS:"), *OpName);
	HeaderOutput += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("FILENAME:"), *FileName);
	HeaderOutput += FString::Printf(TEXT("%*s %s (Frame: %llu)\n"), LabelWidth, TEXT("LOG TIME:"), *FDateTime::Now().ToString(TEXT("%Y.%m.%d - %H:%M:%S")), GFrameCounter);
	HeaderOutput += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("PROJECT:"), *ProjectVer);
	HeaderOutput += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("ACTIVE LEVEL:"), *MapName);

	const ENetMode NetMode = FileCache.CachedNetMode;
	const FString NetModeStr =
		NetMode == NM_Standalone ? TEXT("Standalone") :
		NetMode == NM_DedicatedServer ? TEXT("DedicatedServer") :
		NetMode == NM_ListenServer ? TEXT("ListenServer") :
		NetMode == NM_Client ? TEXT("Client") :
		TEXT("Unknown");
	HeaderOutput += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("NET MODE:"), *NetModeStr);
	
	HeaderOutput += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("UE VERSION:"), *AppVersion);
	HeaderOutput += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("GAME VERSION:"), *BaseContext.GameVersion);
	HeaderOutput += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("EMS VERSION:"), *BaseContext.PluginVersion);

	HeaderOutput += GetSessionBlockSeparator();

	const FString BuildConf = LexToString(FApp::GetBuildConfiguration());
	const FString Platform = FPlatformProperties::IniPlatformName();
	HeaderOutput += FString::Printf(TEXT("%*s %s (%s Build)\n"), LabelWidth, TEXT("ENVIRONMENT:"), *Platform, *BuildConf);

	const uint64 MemMB = FPlatformMemory::GetStats().AvailablePhysical / (1024 * 1024);
	HeaderOutput += FString::Printf(TEXT("%*s %llu MB available\n"), LabelWidth, TEXT("MEMORY:"), MemMB);

	HeaderOutput += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("CPU:"), *FPlatformMisc::GetCPUBrand().TrimStartAndEnd());
	HeaderOutput += FString::Printf(TEXT("%*s %s\n"), LabelWidth, TEXT("GPU:"), *FHardwareInfo::GetHardwareInfo(NAME_RHI));

	HeaderOutput += GetSessionBlockEnd();

	return HeaderOutput;
}

static FString BuildNewObjectPropertiesSnapshot(const FSaveReportContext& Context)
{
	if (!Context.TargetObject)
	{
		return FString();
	}

	//Builds a brand new properties snapshot string for the given target object
	const TArray<FSaveReportData> Props = InspectObjectFields(Context);

	FString NewBlockContent;
	NewBlockContent += BuildObjectDebugHeader(Context, Props.Num());
	NewBlockContent += BuildTransformBlock(Context.TargetObject);

	for (const FSaveReportData& Prop : Props)
	{
		NewBlockContent += BuildPropertyEntryLine(Prop);
	}

	NewBlockContent += GetObjectBlockSplitSignature();

	return NewBlockContent;
}

/**
Advanced Save Data Report - Cache

Global session-level cache. Key is the resolved absolute file path.
Lives for the duration of the in-game session and can be manually cleared. 
By default its called on World cleanup and when the module shuts down.
Can also be cleared and flushed manually via cvars.
**/

static TMap<FString, FReportFileCache> GSaveReportCache;

static FString GetMapName(const UWorld* World)
{
	FString MapName = FPaths::GetBaseFilename(World->GetMapName());

#if WITH_EDITOR
	FEditorPaths::StripRefPIEPrefix(MapName);
#endif

	return MapName;
}

static void CacheWritePropertySnapshot(const FString& FilePath, const FString& PathMarker, const FSaveReportContext& Context, const FString& SnapshotBlock)
{
	FReportFileCache& FileCache = GSaveReportCache.FindOrAdd(FilePath);

	//Capture the context once on first write - used to build the session header at flush time
	if (!FileCache.bDirty)
	{
		FileCache.OperationName = GetOperationTypeName(Context);
		FileCache.BaseContext = Context;

		//Extract World info safely right now while TargetObject is valid
		if (Context.TargetObject)
		{
			if (const UWorld* World = Context.TargetObject->GetWorld())
			{
				FileCache.bHasValidWorld = true;
				FileCache.CachedMapName = GetMapName(World);
				FileCache.CachedNetMode = World->GetNetMode();
			}
		}
	}

	const bool bIsNewEntry = !FileCache.BlocksByPath.Contains(PathMarker);
	if (bIsNewEntry)
	{
		FileCache.InsertionOrder.Add(PathMarker);
	}

	FileCache.BlocksByPath.Add(PathMarker, SnapshotBlock);
	FileCache.bDirty = true;
}

static FString MakeCachePathMarker(const FSaveReportContext& Context)
{
	//This is a unique key for the cached TMap

	if (!Context.TargetObject)
	{
		return TEXT("INVALID");
	}

	if (Context.DataType == ESaveObjectType::CompleteObject)
	{
		//Stable name for Complete objects to prevent dup entries for different outer chain
		const FString FileName = GetDebugFileName(Context);
		FString ClassName = Context.TargetObject->GetClass()->GetPathName();

	#if WITH_EDITOR
		FEditorPaths::StripRefPIEPrefix(ClassName);
	#endif

		return ClassName + EMS::UnderscoreInt + FileName;
	}

	if (const AActor* Actor = Cast<AActor>(Context.TargetObject))
	{
		return FActorHelpers::GetFullActorName(Actor);
	}

	if (const UActorComponent* Comp = Cast<UActorComponent>(Context.TargetObject))
	{
		if (const AActor* Owner = Comp->GetOwner())
		{
			return FActorHelpers::GetFullActorName(Owner) + FActorHelpers::GetComponentName(Owner, Comp);
		}
	}

	FString ObjPathName = Context.TargetObject->GetPathName();

#if WITH_EDITOR
	FEditorPaths::StripRefPIEPrefix(ObjPathName);
#endif

	return ObjPathName;
}

/**
Advanced Save Data Report - Core API
**/

void FSaveReport::WriteSaveReport(const FSaveReportContext& Context)
{
	if (!VerifyDebugSetting(Context))
	{
		return;
	}

	if (!IsValid(Context.TargetObject))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("WriteSaveReport failed: TargetObject in Context is null or invalid."));
		return;
	}

	const FString FileName = GetDebugFileName(Context);
	if (FileName.IsEmpty())
	{
		return;
	}

	const FString DirectoryPath = GetDebugDirectoryPath(Context);
	const FString FilePath = DirectoryPath / (FileName + GetSaveReportFileType());
	const FString PathMarker = MakeCachePathMarker(Context);

	//Build the snapshot block
	const FString SnapshotBlock = BuildNewObjectPropertiesSnapshot(Context);

	//Write into the in-memory cache.
	CacheWritePropertySnapshot(FilePath, PathMarker, Context, SnapshotBlock);
}

void FSaveReport::CommitSaveReport()
{
	/*
	Writes all dirty cache entries to disk (overwriting files) and marks them clean. 
	Cache is not cleared to preserve snapshots for streaming levels.
	*/

	if (GSaveReportCache.Num() <= 0)
	{
		return;
	}

	IFileManager& FileManager = IFileManager::Get();
	bool bWriteSuccess = false;

	for (auto& CachePair : GSaveReportCache)
	{
		const FString& FilePath = CachePair.Key;
		FReportFileCache& FileCache = CachePair.Value;

		if (!FileCache.bDirty)
		{
			continue;
		}

		//Ensure the target directory exists before writing
		const FString DirectoryPath = FPaths::GetPath(FilePath);
		if (!FileManager.DirectoryExists(*DirectoryPath))
		{
			FileManager.MakeDirectory(*DirectoryPath, true);
		}

		//Resolve the file name from the path for the session header
		const FString FileName = FPaths::GetBaseFilename(FilePath);

		//Build the session header fresh here, captures the exact flush timestamp and frame counter
		FString FinalOutput = BuildFileSessionHeader(FileCache, FileName);

		//Append all blocks in first-insertion order
		for (const FString& PathMarker : FileCache.InsertionOrder)
		{
			if (const FString* Block = FileCache.BlocksByPath.Find(PathMarker))
			{
				FinalOutput += *Block;
			}
		}

		//Single write per file, complete overwrite, no merge with prior content
		const bool bSaved = FFileHelper::SaveStringToFile
		(
			FinalOutput,
			*FilePath,
			FFileHelper::EEncodingOptions::AutoDetect,
			&IFileManager::Get(),
			FILEWRITE_None
		);

		if (bSaved)
		{
			FileCache.bDirty = false;
			bWriteSuccess = true;
		}
		else
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("CommitSaveReport: Failed to write debug file at: %s"), *FilePath);
		}
		
	}

	if (bWriteSuccess)
	{
		UE_LOG(LogEasyMultiSave, Log, TEXT("Advanced Save Report written to disk."));
	}
}

void FSaveReport::ClearReportCache(const bool bFullClear)
{
	if (GSaveReportCache.Num() <= 0)
	{
		return;
	}

	UE_LOG(LogEasyMultiSave, Log, TEXT("Advanced Save Report cache cleared."));

	if (bFullClear)
	{
		GSaveReportCache.Empty();
		return;
	}

	/*
	For Complete objects(Slot, Custom) the getter function uses a cache(In EMSObjectBase).
	And so write property info is not called again after the cache is cleared. 
	For saving this is not an issue, but for loading. 
	If we clear the report-cache on level switch, we will only clear it for Actors.
	*/
	for (auto It = GSaveReportCache.CreateIterator(); It; ++It)
	{
		if (It.Value().BaseContext.DataType != ESaveObjectType::CompleteObject)
		{
			It.RemoveCurrent();
		}
	}
}

void FSaveReport::DeleteCurrentSessionFolder()
{
	//Flush any pending cache data to disk before removing files.
	CommitSaveReport();

	const FString SessionDirectory = FPaths::ProjectLogDir() / GetSaveReportFolder() / GetSessionFolder();

	IFileManager& FileManager = IFileManager::Get();
	if (FileManager.DirectoryExists(*SessionDirectory))
	{
		const bool bDeleted = FileManager.DeleteDirectory(*SessionDirectory, true, true);
		if (bDeleted)
		{
			UE_LOG(LogEasyMultiSave, Log, TEXT("Advanced Save Report: Successfully cleared session folder: %s"), *SessionDirectory);
		}
		else
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Advanced Save Report: Failed to delete session folder: %s"), *SessionDirectory);
		}
	}
}

#endif

/**
Debug Logging
**/

void FSaveLogDebug::DebugLogDataSize(const UObject* Object, const TArray<uint8>& InData)
{
	if (!Object)
	{
		return;
	}

	const FString ObjectName = Object->GetName();
	const float SizeKb = InData.Num() / 1024.0f;

	UE_LOG(LogEasyMultiSave, Log, TEXT("Data Size: %.2f Kb | Elements: %d | Object: %s"), SizeKb, InData.Num(), *ObjectName);
}

void FSaveLogDebug::LogActorOperation(const FActorSaveData& ActorData, const bool bIsLoading)
{
	if (!UE_LOG_ACTIVE(LogEasyMultiSave, VeryVerbose))
	{
		return;
	}

	const FString ActorName = FSaveHelpers::StringFromBytes(ActorData.Name);
	const FString ActorClass = FSaveHelpers::StringFromBytes(ActorData.Class);
	const FString TypeName = StaticEnum<EActorType>()->GetNameStringByValue(static_cast<uint8>(ActorData.Type));
	const FString OpName = bIsLoading ? TEXT("Loaded") : TEXT("Saved");

	UE_LOG(LogEasyMultiSave, VeryVerbose,
		TEXT("Actor %s - Name: %s | Type: %s | Class: %s"),
		*OpName,
		*ActorName,
		*TypeName,
		*ActorClass);
}

void FSaveLogDebug::LogPlayerOperation(const AActor* Actor, const TCHAR* PlayerType, const bool bIsLoading, const bool bIsValid)
{
	if (!UE_LOG_ACTIVE(LogEasyMultiSave, VeryVerbose))
	{
		return;
	}

	const FString OpName = bIsLoading ? TEXT("Load") : TEXT("Save");
	const FString TypeName = PlayerType ? FString(PlayerType) : TEXT("Actor");
	const FString ActorName = GetNameSafe(Actor);
	const FString ActorClass = Actor ? GetNameSafe(Actor->GetClass()) : TEXT("None");
	const bool bLoaded = Actor && FActorHelpers::IsLoaded(Actor);

	UE_LOG(LogEasyMultiSave, VeryVerbose,
		TEXT("Attempt Player %s %s - Name: %s | Class: %s | Valid: %s | Loaded: %s"),
		*TypeName,
		*OpName,
		*ActorName,
		*ActorClass,
		bIsValid ? TEXT("Yes") : TEXT("No"),
		bLoaded ? TEXT("Yes") : TEXT("No"));
}

void FSaveLogDebug::LogSavedDestroyedActors(const TArray<FActorSaveData>& DestroyedActors)
{
	if (!UE_LOG_ACTIVE(LogEasyMultiSave, VeryVerbose))
	{
		return;
	}

	for (const FActorSaveData& Destroyed : DestroyedActors)
	{
		LogActorOperation(Destroyed, false);
	}
}
