#include "BTDisplayWidget.h"

FString UBTDisplayWidget::GetMonitorURL() const
{
	return FString::Printf(TEXT("http://localhost:%d"), MonitorPort);
}

void UBTDisplayWidget::TogglePanel()
{
	bExpanded = !bExpanded;
}
