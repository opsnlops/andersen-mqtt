//
// Created by April White on 1/26/25.
//

#pragma once

void setupSerialPort(int serial_port);
void readFromFileDescriptor(int file_descriptor);
void writeToSerial(int serial_port, const std::vector<uint8_t>& bytes);
std::vector<uint8_t> hexStringsToBytes(const std::vector<std::string>& hexStrings);
std::vector<std::string> bytesToHexStrings(const uint8_t* bytes, size_t size);
