/*******************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.  
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#include "lwtr/lwtr_text.cpp"

#include <iostream>
#include <fstream>
#include <string>

int main() {
    // Use a test file name
    const std::string test_file = "test_writer_output.lwtrt";
    // Create a Writer<PlainWriter> and open the file
    lwtr::Writer<lwtr::PlainWriter> writer;
    if (!writer.open(test_file)) {
        std::cerr << "Failed to open test file for writing\n";
        return 1;
    }
    // Write a formatted string
    writer.write("Hello {}! The answer is {}.\n", "world", 42);
    writer.close();

    // Read back the file and check contents
    std::ifstream in(test_file);
    std::string line;
    std::getline(in, line);
    if (line == "Hello world! The answer is 42.") {
        std::cout << "Test passed!\n";
        return 0;
    } else {
        std::cerr << "Test failed! Output: '" << line << "'\n";
        return 2;
    }
}

// Dummy sc_main for SystemC linkage
int sc_main(int, char**) { return 0; }
