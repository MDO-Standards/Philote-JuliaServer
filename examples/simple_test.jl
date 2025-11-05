# Simple test discipline without external dependencies
# This is to test if the C++ Julia integration works at all

# Define a simple discipline type without using external packages
mutable struct SimpleDiscipline
    scale_factor::Float64
    inputs::Dict{String,Tuple{Vector{Int},String}}
    outputs::Dict{String,Tuple{Vector{Int},String}}
    partials::Vector{Tuple{String,String}}

    function SimpleDiscipline()
        new(1.0, Dict(), Dict(), [])
    end
end

# Philote interface functions

function setup!(discipline::SimpleDiscipline)
    # println("SimpleDiscipline.setup!() called")  # Disabled to test if println is causing hang
    discipline.inputs["x"] = ([1], "m")
    discipline.outputs["y"] = ([1], "m")
    return nothing
end

function set_options!(discipline::SimpleDiscipline, options::Dict{String,Any})
    # println disabled to avoid I/O blocking
    if haskey(options, "scale_factor")
        discipline.scale_factor = Float64(options["scale_factor"])
    end
end

function compute(discipline::SimpleDiscipline, inputs::Dict{String,Vector{Float64}})
    # println disabled to avoid I/O blocking
    x = inputs["x"][1]
    y = discipline.scale_factor * x
    return Dict("y" => [y])
end

function declare_partials!(discipline::SimpleDiscipline, output::String, input::String)
    # println disabled to avoid I/O blocking
    push!(discipline.partials, (output, input))
end

function compute_partials(discipline::SimpleDiscipline, inputs::Dict{String,Vector{Float64}})
    # println disabled to avoid I/O blocking
    # Use flat dict with encoded keys: "output~input" => partial_array
    # NOTE: Variable names CANNOT contain '~' character (reserved as delimiter)
    return Dict("y~x" => [discipline.scale_factor])
end

# Simple function to test if we can call Julia functions
function greet()
    # println disabled to avoid I/O blocking
    return "Success"
end
