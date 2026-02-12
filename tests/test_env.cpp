#include <gtest/gtest.h>
#include "valorant/env.hpp"
#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace {

class EnvTest : public ::testing::Test {
protected:
    std::filesystem::path test_env_path = "test_env_file.env";

    void TearDown() override {
        std::filesystem::remove(test_env_path);
    }

    void write_env(const std::string& content) {
        std::ofstream f(test_env_path);
        f << content;
    }
};

} // namespace

TEST_F(EnvTest, ParsesSimpleKeyValue) {
    write_env("MY_KEY=my_value\n");
    auto vars = valorant::load_env(test_env_path);
    EXPECT_EQ(vars["MY_KEY"], "my_value");
}

TEST_F(EnvTest, StripsDoubleQuotes) {
    write_env("QUOTED=\"hello world\"\n");
    auto vars = valorant::load_env(test_env_path);
    EXPECT_EQ(vars["QUOTED"], "hello world");
}

TEST_F(EnvTest, StripsSingleQuotes) {
    write_env("SINGLE='hello world'\n");
    auto vars = valorant::load_env(test_env_path);
    EXPECT_EQ(vars["SINGLE"], "hello world");
}

TEST_F(EnvTest, SkipsComments) {
    write_env("# this is a comment\nKEY=val\n");
    auto vars = valorant::load_env(test_env_path);
    EXPECT_EQ(vars.size(), 1u);
    EXPECT_EQ(vars["KEY"], "val");
}

TEST_F(EnvTest, SkipsEmptyLines) {
    write_env("\n\nKEY=val\n\n");
    auto vars = valorant::load_env(test_env_path);
    EXPECT_EQ(vars.size(), 1u);
}

TEST_F(EnvTest, HandlesSpacesAroundEquals) {
    write_env("  KEY  =  value  \n");
    auto vars = valorant::load_env(test_env_path);
    EXPECT_EQ(vars["KEY"], "value");
}

TEST_F(EnvTest, MultipleVars) {
    write_env("A=1\nB=2\nC=3\n");
    auto vars = valorant::load_env(test_env_path);
    EXPECT_EQ(vars.size(), 3u);
    EXPECT_EQ(vars["A"], "1");
    EXPECT_EQ(vars["B"], "2");
    EXPECT_EQ(vars["C"], "3");
}

TEST_F(EnvTest, MissingFileReturnsEmpty) {
    auto vars = valorant::load_env("nonexistent.env");
    EXPECT_TRUE(vars.empty());
}

TEST_F(EnvTest, DoesNotOverwriteExistingEnv) {
    ::setenv("TEST_EXISTING_VAR", "original", 1);
    write_env("TEST_EXISTING_VAR=overwritten\n");
    valorant::load_env(test_env_path);
    EXPECT_EQ(std::string(::getenv("TEST_EXISTING_VAR")), "original");
    ::unsetenv("TEST_EXISTING_VAR");
}

TEST_F(EnvTest, GetEnvReturnsValue) {
    ::setenv("TEST_GET_ENV", "found", 1);
    auto val = valorant::get_env("TEST_GET_ENV");
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, "found");
    ::unsetenv("TEST_GET_ENV");
}

TEST_F(EnvTest, GetEnvReturnsNulloptForMissing) {
    auto val = valorant::get_env("DEFINITELY_NOT_SET_12345");
    EXPECT_FALSE(val.has_value());
}
