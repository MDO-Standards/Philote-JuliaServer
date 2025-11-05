# Copyright 2025 Christopher A. Lupp
# Licensed under the Apache License, Version 2.0

# Error discipline that throws exceptions on demand for testing error handling

mutable struct ErrorDiscipline
    throw_on_setup::Bool
    throw_on_compute::Bool
    throw_on_partials::Bool
    inputs::Dict{String,Tuple{Vector{Int},String}}
    outputs::Dict{String,Tuple{Vector{Int},String}}

    function ErrorDiscipline()
        new(false, false, false, Dict(), Dict())
    end
end

function setup!(discipline::ErrorDiscipline)
    if discipline.throw_on_setup
        error("Intentional error during setup")
    end

    discipline.inputs["x"] = ([1], "m")
    discipline.outputs["y"] = ([1], "m")
    return nothing
end

function set_options!(discipline::ErrorDiscipline, options::Dict{String,Any})
    if haskey(options, "throw_on_setup")
        discipline.throw_on_setup = Bool(options["throw_on_setup"])
    end
    if haskey(options, "throw_on_compute")
        discipline.throw_on_compute = Bool(options["throw_on_compute"])
    end
    if haskey(options, "throw_on_partials")
        discipline.throw_on_partials = Bool(options["throw_on_partials"])
    end
    return nothing
end

function compute(discipline::ErrorDiscipline, inputs::Dict{String,Vector{Float64}})
    if discipline.throw_on_compute
        error("Intentional error during compute")
    end

    x = inputs["x"][1]
    return Dict("y" => [x * 2.0])
end

function compute_partials(discipline::ErrorDiscipline, inputs::Dict{String,Vector{Float64}})
    if discipline.throw_on_partials
        error("Intentional error during compute_partials")
    end

    return Dict("y~x" => [2.0])
end
