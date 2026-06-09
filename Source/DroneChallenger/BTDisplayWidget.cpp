#include "BTDisplayWidget.h"
#include "Misc/Paths.h"

FString UBTDisplayWidget::GetMonitorURL() const
{
	FString HtmlPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("HUD/arborist/viewer/viewer.html"));
	HtmlPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	return TEXT("file:///") + HtmlPath;
}

void UBTDisplayWidget::TogglePanel()
{
	bExpanded = !bExpanded;
}
