# Copyright 2025 Christopher A. Lupp
# Licensed under the Apache License, Version 2.0

# Multi-output discipline:
# sum = x + y
# product = x * y
# difference = x - y

mutable struct MultiOutputDiscipline
    inputs::Dict{String,Tuple{Vector{Int},String}}
    outputs::Dict{String,Tuple{Vector{Int},String}}

    function MultiOutputDiscipline()
        new(Dict(), Dict())
    end
end

function setup!(discipline::MultiOutputDiscipline)
    discipline.inputs["x"] = ([1], "m")
    discipline.inputs["y"] = ([1], "m")
    discipline.outputs["sum"] = ([1], "m")
    discipline.outputs["product"] = ([1], "m^2")
    discipline.outputs["difference"] = ([1], "m")
    return nothing
end

function set_options!(discipline::MultiOutputDiscipline, options::Dict{String,Any})
    # No options for this discipline
    return nothing
end

function compute(discipline::MultiOutputDiscipline, inputs::Dict{String,Vector{Float64}})
    x = inputs["x"][1]
    y = inputs["y"][1]

    return Dict(
        "sum" => [x + y],
        "product" => [x * y],
        "difference" => [x - y]
    )
end

function compute_partials(discipline::MultiOutputDiscipline, inputs::Dict{String,Vector{Float64}})
    x = inputs["x"][1]
    y = inputs["y"][1]

    # Analytical gradients:
    # ∂sum/∂x = 1, ∂sum/∂y = 1
    # ∂product/∂x = y, ∂product/∂y = x
    # ∂difference/∂x = 1, ∂difference/∂y = -1

    return Dict(
        "sum~x" => [1.0],
        "sum~y" => [1.0],
        "product~x" => [y],
        "product~y" => [x],
        "difference~x" => [1.0],
        "difference~y" => [-1.0]
    )
end
