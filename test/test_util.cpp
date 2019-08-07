#include "gtest/gtest.h"

#include "util.h"

namespace {

TEST(TestUtil, Tokenize) {

    // Arrange
    std::string no_tokens = {""};
    std::string single_token = {"hello"};
    std::string multiple_tokens = {"hello world more tokens"};

    // Act / Assert
    EXPECT_EQ(smldbg::util::tokenize(no_tokens, ' '),
              (std::vector<std::string>{""}));
    EXPECT_EQ(smldbg::util::tokenize(single_token, ' '),
              (std::vector<std::string>{"hello"}));
    EXPECT_EQ(smldbg::util::tokenize(multiple_tokens, ' '),
              (std::vector<std::string>{"hello", "world", "more", "tokens"}));
}

} // namespace