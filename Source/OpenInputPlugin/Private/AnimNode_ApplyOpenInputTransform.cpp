// Fill out your copyright notice in the Description page of Project Settings.
#include "AnimNode_ApplyOpenInputTransform.h"
//#include "EngineMinimal.h"
//#include "Engine/Engine.h"
//#include "CoreMinimal.h"
#include "OpenInputFunctionLibrary.h"
#include "AnimNode_ApplyOpenInputTransform.h"
#include "AnimationRuntime.h"
#include "OpenInputSkeletalMeshComponent.h"
#include "Runtime/Engine/Public/Animation/AnimInstanceProxy.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"

	
FAnimNode_ApplyOpenInputTransform::FAnimNode_ApplyOpenInputTransform()
	: FAnimNode_SkeletalControlBase()
{
	WorldIsGame = false;
	Alpha = 1.f;
	SkeletonType = EVROpenVRSkeletonType::OVR_SkeletonType_UE4Default_Right;
	bIsOpenInputAnimationInstance = false;
}

void FAnimNode_ApplyOpenInputTransform::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	Super::OnInitializeAnimInstance(InProxy, InAnimInstance);

	if (const UOpenInputAnimInstance * animInst = Cast<UOpenInputAnimInstance>(InAnimInstance))
	{
		bIsOpenInputAnimationInstance = true;
	}
}

void FAnimNode_ApplyOpenInputTransform::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	if (!MappedBonePairs.bInitialized)
	{
		USkeleton * AssetSkeleton = RequiredBones.GetSkeletonAsset();

		if (AssetSkeleton)
		{
			if (!MappedBonePairs.BonePairs.Num())
			{
				MappedBonePairs.ConstructDefaultMappings(SkeletonType);
			}

			for (FBPOpenVRSkeletalPair& BonePair : MappedBonePairs.BonePairs)
			{
				// Fill in the bone name for the reference
				BonePair.ReferenceToConstruct.BoneName = BonePair.BoneToTarget;

				// Init the reference
				BonePair.ReferenceToConstruct.Initialize(AssetSkeleton);
				BonePair.ReferenceToConstruct.CachedCompactPoseIndex = BonePair.ReferenceToConstruct.GetCompactPoseIndex(RequiredBones);
			}

			MappedBonePairs.bInitialized = true;
		}
	}
}

void FAnimNode_ApplyOpenInputTransform::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	if (!MappedBonePairs.bInitialized)
		return;

	const FBPOpenVRActionInfo *StoredActionInfoPtr = &OptionalStoredActionInfo;
	if (bIsOpenInputAnimationInstance)
	{
		const FOpenInputAnimInstanceProxy* OpenInputAnimInstance = (FOpenInputAnimInstanceProxy*)Output.AnimInstanceProxy;
		StoredActionInfoPtr = &OpenInputAnimInstance->HandSkeletalActionData;
	}
	
	// Currently not blending correctly
	const float BlendWeight = FMath::Clamp<float>(ActualAlpha, 0.f, 1.f);
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	uint8 BoneTransIndex = 0;
	uint8 NumBones = StoredActionInfoPtr->SkeletalTransforms.Num();//BoneTransforms.Num();

	if (NumBones < 1)
	{
		for (const FBPOpenVRSkeletalPair& BonePair : MappedBonePairs.BonePairs)
		{
			if (BonePair.ReferenceToConstruct.CachedCompactPoseIndex == INDEX_NONE)
				continue;

			if (!BonePair.ReferenceToConstruct.IsValidToEvaluate(BoneContainer))
			{
				continue;
			}
			
			OutBoneTransforms.Emplace(BonePair.ReferenceToConstruct.CachedCompactPoseIndex, Output.Pose.GetComponentSpaceTransform(BonePair.ReferenceToConstruct.CachedCompactPoseIndex));
		}

		return;
	}

	FTransform trans = FTransform::Identity;

	OutBoneTransforms.Reserve(MappedBonePairs.BonePairs.Num());
	TArray<FBoneTransform> TransBones;

	FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();
	FTransform AdditionTransform = StoredActionInfoPtr->AdditionTransform;
	FTransform RootAdditionTransform = StoredActionInfoPtr->RootAdditionTransform;

	FTransform TempTrans = FTransform::Identity;

	for (const FBPOpenVRSkeletalPair& BonePair : MappedBonePairs.BonePairs)
	{
		BoneTransIndex = (int8)BonePair.OpenVRBone;

		if (BoneTransIndex >= NumBones || BonePair.ReferenceToConstruct.CachedCompactPoseIndex == INDEX_NONE)
			continue;		

		if (!BonePair.ReferenceToConstruct.IsValidToEvaluate(BoneContainer))
		{
			continue;
		}

		trans = Output.Pose.GetComponentSpaceTransform(BonePair.ReferenceToConstruct.CachedCompactPoseIndex);

		if (BonePair.ReferenceToConstruct.CachedCompactPoseIndex == 0)
		{
			if (StoredActionInfoPtr->bAllowDeformingMesh)
			{
				TempTrans = RootAdditionTransform * StoredActionInfoPtr->SkeletalTransforms[BoneTransIndex];
				trans.SetTranslation(TempTrans.GetTranslation());
				trans.SetRotation(TempTrans.GetRotation());
			}
			else
			{		
				trans.SetRotation(StoredActionInfoPtr->SkeletalTransforms[BoneTransIndex].GetRotation() * RootAdditionTransform.GetRotation());
			}
		}
		else
		{
			if (StoredActionInfoPtr->bAllowDeformingMesh)
			{
				TempTrans = AdditionTransform * StoredActionInfoPtr->SkeletalTransforms[BoneTransIndex];
				trans.SetTranslation(TempTrans.GetTranslation());
				trans.SetRotation(TempTrans.GetRotation());
			}
			else
			{
				trans.SetRotation(StoredActionInfoPtr->SkeletalTransforms[BoneTransIndex].GetRotation() * AdditionTransform.GetRotation());
			}
		}


		TransBones.Add(FBoneTransform(BonePair.ReferenceToConstruct.CachedCompactPoseIndex, trans));
		Output.Pose.LocalBlendCSBoneTransforms(TransBones, BlendWeight);
		TransBones.Reset();

		//Apply transforms to list
		//OutBoneTransforms.Emplace(BonePair.ReferenceToConstruct.CachedCompactPoseIndex, trans);
	}


}

bool FAnimNode_ApplyOpenInputTransform::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return(MappedBonePairs.bInitialized && MappedBonePairs.BonePairs.Num());
}
