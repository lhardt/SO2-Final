/**
 * This is the main function for the TEST binary.
 * 
 * Please leave it as-is
 */
#include <gtest/gtest.h>

int main(int argc, char **argv){
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}
