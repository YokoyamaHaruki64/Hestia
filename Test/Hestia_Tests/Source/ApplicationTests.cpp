#include <gtest/gtest.h>

#include "Application.h"

TEST(ApplicationFrameSettingsTest, TargetFPSCanBeChanged)
{
    ::testing::Test::RecordProperty("target", "Application::SetTargetFPS, Application::GetTargetFPS");

    Application application;

    application.SetTargetFPS(60.0);
    EXPECT_DOUBLE_EQ(application.GetTargetFPS(), 60.0);

    application.SetTargetFPS(144.0);
    EXPECT_DOUBLE_EQ(application.GetTargetFPS(), 144.0);
}

TEST(ApplicationFrameSettingsTest, UnlimitedFrameRateCanBeEnabledAndDisabled)
{
    ::testing::Test::RecordProperty("target", "Application::SetUnlimitedFrameRate, Application::IsUnlimitedFrameRate");

    Application application;

    application.SetUnlimitedFrameRate(true);
    EXPECT_TRUE(application.IsUnlimitedFrameRate());

    application.SetUnlimitedFrameRate(false);
    EXPECT_FALSE(application.IsUnlimitedFrameRate());
}

TEST(ApplicationFrameSettingsTest, FixedDeltaTimeCanBeChanged)
{
    ::testing::Test::RecordProperty("target", "Application::SetFixedDeltaTime, Application::GetFixedDeltaTime");

    Application application;

    application.SetFixedDeltaTime(1.0 / 30.0);
    EXPECT_DOUBLE_EQ(application.GetFixedDeltaTime(), 1.0 / 30.0);

    application.SetFixedDeltaTime(1.0 / 120.0);
    EXPECT_DOUBLE_EQ(application.GetFixedDeltaTime(), 1.0 / 120.0);
}
