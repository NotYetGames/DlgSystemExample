// Copyright 2017-2018 Csaba Molnar, Daniel Butum
#include "IO/DlgConfigWriter.h"

#include "FileHelper.h"
#include "EnumProperty.h"

DEFINE_LOG_CATEGORY(LogDlgConfigWriter);

const FString DlgConfigWriter::EOL_String = EOL;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DlgConfigWriter::DlgConfigWriter(const UStruct* StructDefinition,
								 const void* Object,
								 const FString& InComplexNamePrefix,
								 bool bInDontWriteEmptyContainer) :
	TopLevelObjectPtr(Object),
	ComplexNamePrefix(InComplexNamePrefix),
	bDontWriteEmptyContainer(bInDontWriteEmptyContainer)
{
	WriteComplexMembersToString(StructDefinition, Object, "", EOL, ConfigText);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DlgConfigWriter::WriteComplexMembersToString(const UStruct* StructDefinition,
												  const void* Object,
												  const FString& PreString,
												  const FString& PostString,
												  FString& Target)
{
	// order
	TArray<const UProperty*> Primitives;
	TArray<const UProperty*> PrimitiveContainers;
	TArray<const UProperty*> ComplexElements;
	TArray<const UProperty*> ComplexContainers;

	constexpr int32 PropCategoryNum = 4;
	TArray<const UProperty*>* PropCategory[PropCategoryNum] = { &Primitives, &PrimitiveContainers, &ComplexElements, &ComplexContainers };

	// Handle UObject inheritance (children of class)
	if (StructDefinition->IsA<UClass>())
	{
		StructDefinition = static_cast<const UObject*>(Object)->GetClass();
	}

	// Populate categories
	for (TFieldIterator<UProperty> It(StructDefinition); It; ++It)
	{
		const UProperty* Property = *It;
		if (IsPrimitive(Property))
		{
			Primitives.Add(Property);
		}
		else if (IsContainer(Property))
		{
			if (IsPrimitiveContainer(Property))
			{
				PrimitiveContainers.Add(Property);
			}
			else
			{
				ComplexContainers.Add(Property);
			}
		}
		else
		{
			ComplexElements.Add(Property);
		}
	}

	constexpr bool bContainerElement = false;
	for (int32 i = 0; i < PropCategoryNum; ++i)
	{
		for (const UProperty* Prop : *PropCategory[i])
		{
			WritePropertyToString(Prop, Object, bContainerElement, PreString, PostString, false, Target);
		}
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::WritePropertyToString(const UProperty* Property,
											const void* Object,
											bool bContainerElement,
											const FString& PreString,
											const FString& PostString,
											bool bPointerAsRef,
											FString& Target)
{
	if (CanSkipProperty(Property))
	{
		return true;
	}

	// Primitive: bool, int, float, FString, FName
	if (WritePrimitiveElementToString(Property, Object, bContainerElement, PreString, PostString, Target))
	{
		return true;
	}

	// Primitive Array: Array[int], Array[bool]
	if (WritePrimitiveArrayToString(Property, Object, PreString, PostString, Target))
	{
		return true;
	}

	// Complex element: UStruct, UObject
	if (WriteComplexElementToString(Property, Object, bContainerElement, PreString, PostString, bPointerAsRef, Target))
	{
		return true;
	}

	// Comples Array: Array[UStruct], Array[UObject]
	if (WriteComplexArrayToString(Property, Object, PreString, PostString, Target))
	{
		return true;
	}

	// Map
	if (WriteMapToString(Property, Object, PreString, PostString, Target))
	{
		return true;
	}

	// Set
	if (WriteSetToString(Property, Object, PreString, PostString, Target))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::WritePrimitiveElementToString(const UProperty* Prop,
													const void* Object,
													bool bInContainer,
													const FString& PreS,
													const FString& PostS,
													FString& Target)
{
	// Try every possible primitive type
	if (WritePrimitiveElementToStringTemplated<UBoolProperty, bool>(Prop, Object, bInContainer, BoolToString, PreS, PostS, Target))
	{
		return true;
	}
	if (WritePrimitiveElementToStringTemplated<UIntProperty, int32>(Prop, Object, bInContainer, IntToString, PreS, PostS, Target))
	{
		return true;
	}
	if (WritePrimitiveElementToStringTemplated<UFloatProperty, float>(Prop, Object, bInContainer, FloatToString, PreS, PostS, Target))
	{
		return true;
	}
	if (WritePrimitiveElementToStringTemplated<UStrProperty, FString>(Prop, Object, bInContainer, StringToString, PreS, PostS, Target))
	{
		return true;
	}
	if (WritePrimitiveElementToStringTemplated<UNameProperty, FName>(Prop, Object, bInContainer, NameToString, PreS, PostS, Target))
	{
		return true;
	}
	if (WritePrimitiveElementToStringTemplated<UTextProperty, FText>(Prop, Object, bInContainer, TextToString, PreS, PostS, Target))
	{
		return true;
	}

	// TODO: enum in container - why isn't it implemented in the reader?
	if (!bInContainer)
	{
		const UEnumProperty* EnumProp = Cast<UEnumProperty>(Prop);
		if (EnumProp != nullptr)
		{
			const void* Value = EnumProp->ContainerPtrToValuePtr<uint8>(Object);
			FName EnumName = EnumProp->GetEnum()->GetNameByIndex(EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value));
			Target += PreS + Prop->GetName() + " " + NameToString(EnumName) + PostS;
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::WritePrimitiveArrayToString(const UProperty* Property,
												  const void* Object,
												  const FString& PreString,
												  const FString& PostString,
												  FString& Target)
{
	const UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
	if (ArrayProp == nullptr)
	{
		return false;
	}

	// Try every possible primitive array type
	if (WritePrimitiveArrayToStringTemplated<UBoolProperty, bool>(ArrayProp, Object, BoolToString, PreString, PostString, Target))
	{
		return true;
	}
	if (WritePrimitiveArrayToStringTemplated<UIntProperty, int32>(ArrayProp, Object, IntToString, PreString, PostString, Target))
	{
		return true;
	}
	if (WritePrimitiveArrayToStringTemplated<UFloatProperty, float>(ArrayProp, Object, FloatToString, PreString, PostString, Target))
	{
		return true;
	}
	if (WritePrimitiveArrayToStringTemplated<UStrProperty, FString>(ArrayProp, Object, StringToString, PreString, PostString, Target))
	{
		return true;
	}
	if (WritePrimitiveArrayToStringTemplated<UNameProperty, FName>(ArrayProp, Object, NameToString, PreString, PostString, Target))
	{
		return true;
	}
	if (WritePrimitiveArrayToStringTemplated<UTextProperty, FText>(ArrayProp, Object, TextToString, PreString, PostString, Target))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::WriteComplexElementToString(const UProperty* Property,
												  const void* Object,
												  bool bContainerElement,
												  const FString& PreString,
												  const FString& PostString,
												  bool bPointerAsRef,
												  FString& Target)
{
	// UStruct
	const UStructProperty* StructProp = Cast<UStructProperty>(Property);
	if (StructProp != nullptr)
	{
		WriteComplexToString(StructProp->Struct,
							 Property,
							 StructProp->ContainerPtrToValuePtr<void>(Object, 0),
							 PreString,
							 PostString,
							 bContainerElement,
							 false,
							 Target);
		return true;
	}

	// UObject
	const UObjectProperty* ObjProp = Cast<UObjectProperty>(Property);
	if (ObjProp != nullptr)
	{
		UObject** ObjPtrPtr = ((UObject**)ObjProp->ContainerPtrToValuePtr<void>(Object, 0));
		if (CanSaveAsReference(ObjProp) || bPointerAsRef)
		{
			const FString Path = (*ObjPtrPtr != nullptr ? (*ObjPtrPtr)->GetPathName() : "");
			if (bContainerElement)
			{
				Target += PreString + "\"" + Path + "\"" + PostString;
			}
			else
			{
				if (*ObjPtrPtr == nullptr)
				{
					return true;
				}
				Target += PreString +  Property->GetName() + " \"" + Path + "\"" + PostString;
			}
		}
		else
		{
			// nullptr-s are not written
			if (*ObjPtrPtr == nullptr)
			{
				return true;
			}

			WriteComplexToString(ObjProp->PropertyClass,
								 Property,
								 *ObjPtrPtr,
								 PreString,
								 PostString,
								 bContainerElement,
								 true,
								 Target);
		}
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DlgConfigWriter::WriteComplexToString(const UStruct* StructDefinition,
										   const UProperty* Property,
										   const void* Object,
										   const FString& PreString,
										   const FString& PostString,
										   bool bContainerElement,
										   bool bWriteType,
										   FString& Target)
{
	if (CanSkipProperty(Property))
	{
		return;
	}

	const bool bLinePerMember = WouldWriteNonPrimitive(StructDefinition, Object);

	// WARNING: bWriteType implicates objectproperty, if that changes this code (cause of the object cast) should be updated accordingly
	const FString TypeString = bWriteType ? GetNameWithoutPrefix(Property, static_cast<const UObject*>(Object)) + " ": "";
	if (bContainerElement)
	{
		if (TypeString.Len() > 0)
		{
			Target += PreString + TypeString + (bLinePerMember ? EOL + PreString + "{" + EOL : "{ ");
		}
		else
		{
			Target += PreString + (bLinePerMember ? "{" + EOL_String : "{ ");
		}
	}
	else
	{
		Target += PreString + TypeString + Property->GetName() + (bLinePerMember ? EOL + PreString + "{" + EOL : " { ");
	}

	// Write the properties of the Struct/Object
	WriteComplexMembersToString(StructDefinition, Object, (bLinePerMember ? PreString + "\t" : " "), (bLinePerMember ? EOL_String : ""), Target);

	if (bLinePerMember)
	{
		Target += PreString + "}" + PostString;
	}
	else
	{
		Target += " }" + PostString;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::WriteComplexArrayToString(const UProperty* Property,
												const void* Object,
												const FString& PreString,
												const FString& PostString,
												FString& Target)
{
	const UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
	if (ArrayProp == nullptr)
	{
		return false;
	}
	if (Cast<UStructProperty>(ArrayProp->Inner) == nullptr && Cast<UObjectProperty>(ArrayProp->Inner) == nullptr)
	{
		return false;
	}

	// Empty Array
	FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<uint8>(Object));
	if (Helper.Num() == 0 && bDontWriteEmptyContainer)
	{
		return true;
	}

	const bool bWriteIndex = CanWriteIndex(Property);
	FString TypeText = "";
	UObjectProperty* ObjProp = Cast<UObjectProperty>(ArrayProp->Inner);
	if (ObjProp != nullptr && ObjProp->PropertyClass != nullptr)
	{
		TypeText = GetStringWithoutPrefix(ObjProp->PropertyClass->GetName()) + " ";
	}

	if (Helper.Num() == 1 && !WouldWriteNonPrimitive(GetComplexType(ArrayProp->Inner), Helper.GetRawPtr(0)))
	{
		Target += PreString + TypeText + ArrayProp->Inner->GetName() + " {";
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			WriteComplexElementToString(ArrayProp->Inner, Helper.GetRawPtr(i), true, " ", "", CanSaveAsReference(ArrayProp), Target);
		}
		Target += " }" + PostString;
	}
	else
	{
		Target += PreString + TypeText + ArrayProp->Inner->GetName() + EOL;
		Target += PreString + "{" + EOL;
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			if (bWriteIndex)
			{
				Target += PreString + "\t// " + FString::FromInt(i) + EOL;
			}
			WriteComplexElementToString(ArrayProp->Inner, Helper.GetRawPtr(i), true, PreString + "\t", EOL, CanSaveAsReference(ArrayProp), Target);
		}
		Target += PreString + "}" + EOL;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::WriteMapToString(const UProperty* Property,
									   const void* Object,
									   const FString& PreString,
									   const FString& PostString,
									   FString& Target)
{
	const UMapProperty* MapProp = Cast<UMapProperty>(Property);
	if (MapProp == nullptr)
	{
		return false;
	}

	// Empty map
	FScriptMapHelper Helper(MapProp, MapProp->ContainerPtrToValuePtr<uint8>(Object));
	if (Helper.Num() == 0 && bDontWriteEmptyContainer)
	{
		return true;
	}

	if (IsPrimitive(MapProp->KeyProp) && IsPrimitive(MapProp->ValueProp))
	{
		// Both Key and Value are primitives
		Target += PreString + MapProp->GetName() + " { ";
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			WritePrimitiveElementToString(MapProp->KeyProp, Helper.GetPairPtr(i), true, "", " ", Target);
			WritePrimitiveElementToString(MapProp->ValueProp, Helper.GetPairPtr(i), true, "", " ", Target);
		}
		Target += "}" + PostString;
	}
	else
	{
		// Either Key or Value is not a primitive
		Target += PreString + MapProp->GetName() + EOL;
		Target += PreString + "{" + EOL;
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			WritePropertyToString(MapProp->KeyProp, Helper.GetKeyPtr(i), true, PreString + "\t", EOL, CanSaveAsReference(MapProp), Target);
			WritePropertyToString(MapProp->ValueProp, Helper.GetKeyPtr(i), true, PreString + "\t", EOL, CanSaveAsReference(MapProp), Target);
		}
		Target += PreString + "}" + EOL;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::WriteSetToString(const UProperty* Property,
									   const void* Object,
									   const FString& PreString,
									   const FString& PostString,
									   FString& Target)
{
	const USetProperty* SetProp = Cast<USetProperty>(Property);
	if (SetProp == nullptr)
	{
		return false;
	}

	// Empty set
	FScriptSetHelper Helper(SetProp, SetProp->ContainerPtrToValuePtr<uint8>(Object));
	if (Helper.Num() == 0 && bDontWriteEmptyContainer)
	{
		return true;
	}

	// Only write primitive set elements
	if (IsPrimitive(SetProp->ElementProp))
	{
		const bool bLinePerItem = CanWriteOneLinePerItem(SetProp);

		// Add space indentation
		FString SubPreString = PreString;
		for (int32 i = 0; i < SetProp->GetName().Len() + 3; ++i)
		{
			SubPreString += " ";
		}

		// SetName {
		Target += PreString + SetProp->GetName() + " {";
		if (!bLinePerItem)
		{
			// Add space because there is no new line
			Target += " ";
		}

		// Set content
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			if (bLinePerItem)
			{
				WritePrimitiveElementToString(SetProp->ElementProp, Helper.GetElementPtr(i), true, EOL + SubPreString, "", Target);
			}
			else
			{
				WritePrimitiveElementToString(SetProp->ElementProp, Helper.GetElementPtr(i), true, "", " ", Target);
			}
		}

		// }
		Target += (bLinePerItem ? " " : "") + FString("}") + PostString;
	}
	else
	{
		UE_LOG(LogDlgConfigWriter, Warning, TEXT("Set not exported: unsuported set member (%s)"), *SetProp->ElementProp->GetName());
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::IsPrimitive(const UProperty* Property)
{
	return Cast<UBoolProperty>(Property) != nullptr ||
		   Cast<UIntProperty>(Property) != nullptr ||
		   Cast<UFloatProperty>(Property) != nullptr ||
		   Cast<UStrProperty>(Property) != nullptr ||
		   Cast<UNameProperty>(Property) != nullptr ||
		   Cast<UTextProperty>(Property) != nullptr ||
		   Cast<UEnumProperty>(Property) != nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::IsContainer(const UProperty* Property)
{
	return Cast<UArrayProperty>(Property) != nullptr ||
		   Cast<UMapProperty>(Property) != nullptr ||
		   Cast<USetProperty>(Property) != nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::IsPrimitiveContainer(const UProperty* Property)
{
	// Array
	if (Cast<UArrayProperty>(Property) != nullptr && IsPrimitive(Cast<UArrayProperty>(Property)->Inner))
	{
		return true;
	}

	// Map
	if (Cast<UMapProperty>(Property) != nullptr &&
		IsPrimitive(Cast<UMapProperty>(Property)->KeyProp) &&
		IsPrimitive(Cast<UMapProperty>(Property)->ValueProp))
	{
		return true;
	}

	// Set
	if (Cast<USetProperty>(Property) != nullptr && IsPrimitive(Cast<USetProperty>(Property)->ElementProp))
	{
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const UStruct* DlgConfigWriter::GetComplexType(const UProperty* Property)
{
	const UStructProperty* StructProp = Cast<UStructProperty>(Property);
	if (StructProp != nullptr)
	{
		return StructProp->Struct;
	}

	const UObjectProperty* ObjectProp = Cast<UObjectProperty>(Property);
	if (ObjectProp != nullptr)
	{
		return ObjectProp->PropertyClass;
	}

	return nullptr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool DlgConfigWriter::WouldWriteNonPrimitive(const UStruct* StructDefinition, const void* Owner)
{
	if (StructDefinition == nullptr)
	{
		return false;
	}

	for (TFieldIterator<const UProperty> It(StructDefinition); It; ++It)
	{
		const UProperty* Property = *It;

		// Ignore primitives
		if (IsPrimitive(Property))
		{
			continue;
		}

		if (IsContainer(Property))
		{
			// Map
			if (const UMapProperty* MapProperty = Cast<UMapProperty>(Property))
			{
				FScriptMapHelper Helper(MapProperty, Property->ContainerPtrToValuePtr<uint8>(Owner));
				if (Helper.Num() > 0)
				{
					return true;
				}
			}

			// Array
			if (const UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
			{
				FScriptArrayHelper Helper(ArrayProperty, Property->ContainerPtrToValuePtr<uint8>(Owner));
				if (Helper.Num() > 0)
				{
					return true;
				}
			}

			// Set
			if (const USetProperty* SetProperty = Cast<USetProperty>(Property))
			{
				FScriptSetHelper Helper(SetProperty, Property->ContainerPtrToValuePtr<uint8>(Owner));
				if (Helper.Num() > 0)
				{
					return true;
				}
			}
		}
		else
		{
			// Struct or Object
			if (Cast<UStructProperty>(Property) != nullptr || Cast<UObjectProperty>(Property) != nullptr)
			{
				return true;
			}
		}

	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FString DlgConfigWriter::GetNameWithoutPrefix(const UProperty* Property, const UObject* ObjectPtr)
{
	const UStructProperty* StructProp = Cast<UStructProperty>(Property);
	if (StructProp != nullptr)
	{
		return GetStringWithoutPrefix(StructProp->Struct->GetName());
	}

	const UObjectProperty* ObjectProp = Cast<UObjectProperty>(Property);
	if (ObjectProp != nullptr)
	{
		if (ObjectPtr == nullptr)
		{
			return GetStringWithoutPrefix(ObjectProp->PropertyClass->GetName());
		}

		return GetStringWithoutPrefix(ObjectPtr->GetClass()->GetName());
	}

	return "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FString DlgConfigWriter::GetStringWithoutPrefix(const FString& String)
{
	int32 Count = String.Len();
	for (int32 i = 0; i < ComplexNamePrefix.Len() && i < String.Len() && ComplexNamePrefix[i] == String[i]; ++i)
	{
		Count -= 1;
	}

	return String.Right(Count);
}