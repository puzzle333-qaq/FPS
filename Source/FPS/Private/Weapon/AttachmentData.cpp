#include "Weapon/AttachmentData.h"

FPrimaryAssetId UAttachmentData::GetPrimaryAssetId() const
{
	// 类型名 "Attachment" + 资产名，例如 Attachment:DA_RedDot
	return FPrimaryAssetId(TEXT("Attachment"), GetFName());
}
