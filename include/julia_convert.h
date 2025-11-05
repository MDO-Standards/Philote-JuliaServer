// Copyright 2025 MDO Standards
// Licensed under the Apache License, Version 2.0

#ifndef PHILOTE_JULIA_SERVER_JULIA_CONVERT_H
#define PHILOTE_JULIA_SERVER_JULIA_CONVERT_H

#include <google/protobuf/struct.pb.h>
#include <julia.h>

#include <string>

#include <variable.h>

namespace philote {
namespace julia {

/**
 * @brief Convert Philote Variables to Julia Dict{String, Array{Float64}}
 *
 * Converts a map of Variable objects to a Julia dictionary where keys are
 * variable names and values are Julia arrays.
 *
 * @param vars Philote Variables to convert
 * @return Julia Dict object
 * @throws std::runtime_error if conversion fails
 *
 * @note Caller is responsible for GC protection of returned object
 */
jl_value_t* VariablesToJuliaDict(const philote::Variables& vars);

/**
 * @brief Convert Julia Dict to Philote Variables
 *
 * Converts a Julia Dict{String, Array{Float64}} to Philote Variables map.
 *
 * @param dict Julia dictionary to convert
 * @return Philote Variables object
 * @throws std::runtime_error if conversion fails or dict has wrong type
 */
philote::Variables JuliaDictToVariables(jl_value_t* dict);

/**
 * @brief Convert Julia Dict to Philote Partials
 *
 * Converts a Julia Dict{Tuple{String,String}, Array{Float64}} to Philote
 * Partials map. Keys are (output, input) tuples.
 *
 * @param dict Julia dictionary to convert
 * @return Philote Partials object
 * @throws std::runtime_error if conversion fails or dict has wrong type
 */
philote::Partials JuliaDictToPartials(jl_value_t* dict);

/**
 * @brief Convert protobuf Struct to Julia Dict
 *
 * Converts a protobuf Struct (used for options) to a Julia Dict.
 * Supports number, bool, and string values.
 *
 * @param s Protobuf Struct to convert
 * @return Julia Dict object
 * @throws std::runtime_error if conversion fails
 *
 * @note Caller is responsible for GC protection of returned object
 */
jl_value_t* ProtobufStructToJuliaDict(const google::protobuf::Struct& s);

/**
 * @brief Check if a Julia exception occurred and throw C++ exception
 *
 * Helper function to check jl_exception_occurred() and convert Julia
 * exception to C++ std::runtime_error.
 *
 * @throws std::runtime_error if Julia exception occurred
 */
void CheckJuliaException();

/**
 * @brief Get string representation of Julia exception
 *
 * @return String describing the Julia exception
 */
std::string GetJuliaExceptionString();

}  // namespace julia
}  // namespace philote

#endif  // PHILOTE_JULIA_SERVER_JULIA_CONVERT_H
