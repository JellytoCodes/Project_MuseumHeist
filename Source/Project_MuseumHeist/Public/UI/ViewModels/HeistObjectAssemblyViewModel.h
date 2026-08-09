#pragma once

#include "CoreMinimal.h"
#include "Core/HeistTypes.h"
#include "Data/HeistArtifactDataTypes.h"
#include "MVVMViewModelBase.h"

#include "HeistObjectAssemblyViewModel.generated.h"

class AHeistGameState;
class AHeistPlayerController;
class UHeistObjectAssemblyComponent;
class UStaticMesh;

DECLARE_MULTICAST_DELEGATE(FHeistObjectAssemblyPresentationChanged);

/**
 * Owner-local Object Assembly presentation and request routing.
 *
 * Static meshes and local preview state never cross the network boundary.
 * Submit requests contain compact FHeistObjectAssemblyEntry values only.
 */
UCLASS(BlueprintType)
class PROJECT_MUSEUMHEIST_API UHeistObjectAssemblyViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

#pragma region Lifecycle

  protected:
	virtual void BeginDestroy() override;

#pragma endregion

#pragma region Setup

  public:
	void SetupViewModel(AHeistGameState* InGameState, UHeistObjectAssemblyComponent* InObjectAssemblyComponent, AHeistPlayerController* InPlayerController);
	void RefreshPresentationState();
	FHeistObjectAssemblyPresentationChanged& GetPresentationChangedDelegate();

  private:
	void HandleAssemblySessionStateChanged();
	void HandleAlertStateChanged(EHeistAlertLevel PreviousAlertLevel, EHeistAlertLevel NewAlertLevel, int32 AlertRevision, FName TriggerId);
	bool LoadActiveTemplateData();
	void ClearLoadedTemplateData();
	void ResetLocalAssemblyState();
	void SyncSelectionFromSelectedPart();
	void RefreshSelectionPresentation();
	void RefreshQualityPreview();
	void RefreshAlertPresentation();
	void SetStatusMessage(const FText& NewStatusText);
	const FHeistObjectAssemblyPartRow* FindPartDefinition(FName PartId) const;
	const FHeistObjectAssemblyEntry* FindLocalEntry(FName PartId) const;
	FHeistObjectAssemblyEntry* FindMutableLocalEntry(FName PartId);
	static FText MakeIdentifierDisplayText(FName Identifier);
	static FText MakePayloadReasonText(FName Reason);

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> GameState;

	UPROPERTY(Transient)
	TObjectPtr<UHeistObjectAssemblyComponent> ObjectAssemblyComponent;

	UPROPERTY(Transient)
	TObjectPtr<AHeistPlayerController> PlayerController;

	FHeistObjectAssemblyPresentationChanged PresentationChangedDelegate;
	FHeistObjectAssemblyTemplateRow ActiveTemplate;
	TMap<FName, FHeistObjectAssemblyPartRow> PartDefinitions;
	TArray<FName> CandidatePartIds;
	TArray<FHeistObjectAssemblyEntry> LocalAssemblyEntries;
	int32 SelectedPartIndex = INDEX_NONE;
	int32 SelectedSocketIndex = INDEX_NONE;
	int32 SelectedOrientationIndex = INDEX_NONE;
	int32 SelectedMaterialIndex = INDEX_NONE;
	int32 LoadedSessionRevision = INDEX_NONE;
	int32 ObservedPayloadValidationRevision = 0;
	int32 LocalPreviewRevision = 0;
	bool bSubmitPending = false;
	bool bHasPreviewQuality = false;
	float PreviewQualityScore = 0.0f;
	float MinimumAcceptedQualityScore = HeistReplicaAcceptance::MinimumQualityScore;

#pragma endregion

#pragma region LocalAssembly

  public:
	bool SelectPreviousPart();
	bool SelectNextPart();
	bool SelectPreviousSocket();
	bool SelectNextSocket();
	bool RotatePrevious();
	bool RotateNext();
	bool PlaceOrUpdateSelectedPart();
	bool RemoveSelectedPart();
	bool PlacePartAtSocket(FName PartId, FName SocketId);
	bool RemovePart(FName PartId);
	bool RotatePart(FName PartId, int32 Direction);
	bool ResetLocalAssembly();
	bool RequestSubmitAssembly();
	bool RequestCancelAssembly();

	bool IsPresentationVisible() const;
	bool IsDataReady() const;
	bool IsOwnerOnlyContractSatisfied() const;
	bool IsSubmitPending() const;
	float GetSessionEndServerTime() const;
	int32 GetSessionRevision() const;
	FName GetActiveArtifactId() const;
	FName GetActiveTemplateId() const;
	FName GetActiveFamilyId() const;
	const FHeistObjectAssemblyTemplateRow& GetActiveTemplate() const;
	const TArray<FName>& GetCandidatePartIds() const;
	const TArray<FName>& GetCompatibleSocketIds(FName PartId) const;
	const TArray<FHeistObjectAssemblyEntry>& GetLocalAssemblyEntries() const;
	bool IsPartPlaced(FName PartId) const;
	uint8 GetPlacedPartOrientation(FName PartId) const;
	FText GetPartDisplayText(FName PartId) const;
	FName GetSelectedPartId() const;
	FName GetSelectedSocketId() const;
	uint8 GetSelectedOrientation() const;
	FName GetSelectedMaterialId() const;
	int32 GetCandidatePartCount() const;
	int32 GetPlacedPartCount() const;
	int32 GetRequiredPartCount() const;
	int32 GetPlacedRequiredPartCount() const;
	int32 GetLocalPreviewRevision() const;
	bool HasPreviewQuality() const;
	float GetPreviewQualityScore() const;
	float GetMinimumAcceptedQualityScore() const;
	bool CanSubmitAssembly() const;
	UStaticMesh* LoadCoreStaticMesh() const;
	UStaticMesh* LoadPartStaticMesh(FName PartId) const;

#pragma endregion

#pragma region Presentation

  public:
	const FText& GetTemplateDisplayText() const;
	const FText& GetSelectedPartText() const;
	const FText& GetSelectedSocketText() const;
	const FText& GetSelectedOrientationText() const;
	const FText& GetPlacementProgressText() const;
	const FText& GetStatusText() const;
	EHeistAlertLevel GetAlertLevel() const;
	bool IsDangerWarningVisible() const;
	const FText& GetDangerWarningText() const;
	FLinearColor GetDangerWarningColor() const;
	bool IsLockdownCountdownVisible() const;
	float GetLockdownCountdownEndServerTime() const;

  private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	bool bPresentationVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	bool bDataReady = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	float SessionEndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FText TemplateDisplayText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FText SelectedPartText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FText SelectedSocketText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FText SelectedOrientationText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FText PlacementProgressText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Object Assembly", meta = (AllowPrivateAccess = "true"))
	FText StatusText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	EHeistAlertLevel AlertLevel = EHeistAlertLevel::Quiet;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	bool bDangerWarningVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	FText DangerWarningText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	FLinearColor DangerWarningColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	bool bLockdownCountdownVisible = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Heist|Alert", meta = (AllowPrivateAccess = "true"))
	float LockdownCountdownEndServerTime = 0.0f;

#pragma endregion
};
