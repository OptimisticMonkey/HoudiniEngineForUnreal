/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngine.h"

#include "HoudiniSplineComponentVisualizer.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniRuntimeSettingsDetails.h"
#include "HoudiniAssetTypeActions.h"
#include "HoudiniAssetBroker.h"
#include "HoudiniAssetActorFactory.h"


const FName
FHoudiniEngineEditor::HoudiniEngineEditorAppIdentifier = FName(TEXT("HoudiniEngineEditorApp"));


IMPLEMENT_MODULE(FHoudiniEngineEditor, HoudiniEngineEditor);
DEFINE_LOG_CATEGORY(LogHoudiniEngineEditor);


FHoudiniEngineEditor*
FHoudiniEngineEditor::HoudiniEngineEditorInstance = nullptr;


FHoudiniEngineEditor&
FHoudiniEngineEditor::Get()
{
	return *HoudiniEngineEditorInstance;
}


bool
FHoudiniEngineEditor::IsInitialized()
{
	return FHoudiniEngineEditor::HoudiniEngineEditorInstance != nullptr;
}


void
FHoudiniEngineEditor::StartupModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Starting the Houdini Engine Editor module."));

	// Load Houdini Engine Runtime module.
	//IHoudiniEngine& HoudiniEngine = FModuleManager::LoadModuleChecked<FHoudiniEngine>("HoudiniEngine").Get();

	// Register asset type actions.
	RegisterAssetTypeActions();

	// Register asset brokers.
	RegisterAssetBrokers();

	// Register component visualizers.
	RegisterComponentVisualizers();

	// Register detail presenters.
	RegisterDetails();

	// Register actor factories.
	RegisterActorFactories();

	// Create style set.
	RegisterStyleSet();

	// Extend menu.
	ExtendMenu();

	// Store the instance.
	FHoudiniEngineEditor::HoudiniEngineEditorInstance = this;
}


void
FHoudiniEngineEditor::ShutdownModule()
{
	HOUDINI_LOG_MESSAGE(TEXT("Shutting down the Houdini Engine Editor module."));

	// Unregister asset type actions.
	UnregisterAssetTypeActions();

	// Unregister detail presenters.
	UnregisterDetails();

	// Unregister our component visualizers.
	UnregisterComponentVisualizers();
}


void
FHoudiniEngineEditor::RegisterComponentVisualizers()
{
	if(GUnrealEd && !SplineComponentVisualizer.IsValid())
	{
		SplineComponentVisualizer = MakeShareable(new FHoudiniSplineComponentVisualizer);
		GUnrealEd->RegisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName(),
			SplineComponentVisualizer);

		SplineComponentVisualizer->OnRegister();
	}
}


void
FHoudiniEngineEditor::UnregisterComponentVisualizers()
{
	if(GUnrealEd && SplineComponentVisualizer.IsValid())
	{
		GUnrealEd->UnregisterComponentVisualizer(UHoudiniSplineComponent::StaticClass()->GetFName());
	}
}


void
FHoudiniEngineEditor::RegisterDetails()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Register details presenter for our component type and runtime settings.
	PropertyModule.RegisterCustomClassLayout(TEXT("HoudiniAssetComponent"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniAssetComponentDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(TEXT("HoudiniRuntimeSettings"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniRuntimeSettingsDetails::MakeInstance));
}


void
FHoudiniEngineEditor::UnregisterDetails()
{
	if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.UnregisterCustomClassLayout(TEXT("HoudiniAssetComponent"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("HoudiniRuntimeSettings"));
	}
}


void
FHoudiniEngineEditor::RegisterAssetTypeActions()
{
	// Create and register asset type actions for Houdini asset.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FHoudiniAssetTypeActions()));
}


void
FHoudiniEngineEditor::UnregisterAssetTypeActions()
{
	// Unregister asset type actions we have previously registered.
	if(FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		for(int32 Index = 0; Index < AssetTypeActions.Num(); ++Index)
		{
			AssetTools.UnregisterAssetTypeActions(AssetTypeActions[Index].ToSharedRef());
		}

		AssetTypeActions.Empty();
	}
}


void
FHoudiniEngineEditor::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	AssetTypeActions.Add(Action);
}


void
FHoudiniEngineEditor::RegisterAssetBrokers()
{
	// Create and register broker for Houdini asset.
	HoudiniAssetBroker = MakeShareable(new FHoudiniAssetBroker());
	FComponentAssetBrokerage::RegisterBroker(HoudiniAssetBroker, UHoudiniAssetComponent::StaticClass(), true, true);
}


void
FHoudiniEngineEditor::UnregisterAssetBrokers()
{
	if(UObjectInitialized())
	{
		// Unregister broker.
		FComponentAssetBrokerage::UnregisterBroker(HoudiniAssetBroker);
	}
}


void
FHoudiniEngineEditor::RegisterActorFactories()
{
	if(GEditor)
	{
		UHoudiniAssetActorFactory* HoudiniAssetActorFactory =
			ConstructObject<UHoudiniAssetActorFactory>(UHoudiniAssetActorFactory::StaticClass());

		GEditor->ActorFactories.Add(HoudiniAssetActorFactory);
	}
}


void
FHoudiniEngineEditor::ExtendMenu()
{
	if(!IsRunningCommandlet())
	{
		// Extend main menu, we will add Houdini section.
		MainMenuExtender = MakeShareable(new FExtender);
		MainMenuExtender->AddMenuExtension("FileLoadAndSave", EExtensionHook::After, NULL,
			FMenuExtensionDelegate::CreateRaw(this, &FHoudiniEngineEditor::AddHoudiniMenuExtension));
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);
	}
}


void
FHoudiniEngineEditor::AddHoudiniMenuExtension(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Houdini", LOCTEXT("HoudiniLabel", "Houdini Engine"));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("HoudiniMenuEntryTitle", "Save .hip file"),
			LOCTEXT("HoudiniMenuEntryToolTip", "Saves a .hip file of the current Houdini scene."),
			FSlateIcon(StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo"),
			FUIAction(FExecuteAction::CreateRaw(this, &FHoudiniEngineEditor::SaveHIPFile),
				FCanExecuteAction::CreateRaw(this, &FHoudiniEngineEditor::CanSaveHIPFile)));

	MenuBuilder.EndSection();
}


bool
FHoudiniEngineEditor::CanSaveHIPFile() const
{
	return FHoudiniEngine::IsInitialized();
}


void
FHoudiniEngineEditor::SaveHIPFile()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if(DesktopPlatform && FHoudiniEngineUtils::IsInitialized())
	{
		TArray<FString> SaveFilenames;
		bool bSaved = false;
		void* ParentWindowWindowHandle = NULL;

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		if(MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		bSaved = DesktopPlatform->SaveFileDialog(ParentWindowWindowHandle,
			NSLOCTEXT("SaveHIPFile", "SaveHIPFile", "Saves a .hip file of the current Houdini scene.").ToString(),
			*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)),
			TEXT(""),
			TEXT("Houdini HIP file|*.hip"),
			EFileDialogFlags::None,
			SaveFilenames);

		if(bSaved && SaveFilenames.Num())
		{
			// Get first path.
			std::wstring HIPPath(*SaveFilenames[0]);
			std::string HIPPathConverted(HIPPath.begin(), HIPPath.end());

			// Save HIP file through Engine.
			FHoudiniApi::SaveHIPFile(HIPPathConverted.c_str(), false);
		}
	}
}


void
FHoudiniEngineEditor::RegisterStyleSet()
{
	// Create Slate style set.
	if(!StyleSet.IsValid())
	{
		StyleSet = MakeShareable(new FSlateStyleSet("HoudiniEngineStyle"));
		StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
		StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		const FVector2D Icon16x16(16.0f, 16.0f);
		static FString ContentDir = FPaths::EnginePluginsDir() / TEXT("Runtime/HoudiniEngine/Content/Icons/");
		StyleSet->Set("HoudiniEngine.HoudiniEngineLogo",
			new FSlateImageBrush(ContentDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));
		StyleSet->Set("ClassIcon.HoudiniAssetActor",
			new FSlateImageBrush(ContentDir + TEXT("icon_houdini_logo_16.png"), Icon16x16));

		// Register Slate style.
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());

		// Register style set as an icon source.
		FClassIconFinder::RegisterIconSource(StyleSet.Get());
	}
}


void
FHoudiniEngineEditor::UnregisterStyleSet()
{
	// Unregister Slate style set.
	if(StyleSet.IsValid())
	{
		// Unregister style set as an icon source.
		FClassIconFinder::UnregisterIconSource(StyleSet.Get());

		// Unregister Slate style.
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());

		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}


TSharedPtr<ISlateStyle>
FHoudiniEngineEditor::GetSlateStyle() const
{
	return StyleSet;
}
