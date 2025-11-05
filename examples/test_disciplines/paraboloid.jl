# Copyright 2025 Christopher A. Lupp
# Licensed under the Apache License, Version 2.0

# Paraboloid discipline: f(x, y) = x² + y²
# This discipline has known analytical gradients for testing

mutable struct ParaboloidDiscipline
    inputs::Dict{String,Tuple{Vector{Int},String}}
    outputs::Dict{String,Tuple{Vector{Int},String}}

    function ParaboloidDiscipline()
        new(Dict(), Dict())
    end
end

function setup!(discipline::ParaboloidDiscipline)
    discipline.inputs["x"] = ([1], "m")
    discipline.inputs["y"] = ([1], "m")
    discipline.outputs["f"] = ([1], "m^2")
    return nothing
end

function set_options!(discipline::ParaboloidDiscipline, options::Dict{String,Any})
    # No options for this simple discipline
    return nothing
end

function compute(discipline::ParaboloidDiscipline, inputs::Dict{String,Vector{Float64}})
    x = inputs["x"][1]
    y = inputs["y"][1]
    f = x^2 + y^2
    return Dict("f" => [f])
end

function compute_partials(discipline::ParaboloidDiscipline, inputs::Dict{String,Vector{Float64}})
    x = inputs["x"][1]
    y = inputs["y"][1]

    # Analytical gradients:
    # ∂f/∂x = 2x
    # ∂f/∂y = 2y

    return Dict(
        "f~x" => [2.0 * x],
        "f~y" => [2.0 * y]
    )
end
