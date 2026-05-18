#include "agent.h"
#include <string>
#include "tick.h"
#include <algorithm>
#include <limits>
#include <nlohmann/json.hpp>

void from_json(const nlohmann::json& j, Recipe& recipe) {
    j.at("inputs").get_to(recipe.inputs);
    j.at("outputs").get_to(recipe.outputs);
    recipe.cost = j.value("cost", 0L);
}


std::vector<Recipe> parseRecipesJson(const std::string& jsonText) {
    const nlohmann::json parsed = nlohmann::json::parse(jsonText);

    if (parsed.is_array()) {
        return parsed.get<std::vector<Recipe>>();
    }

    if (parsed.is_object()) {
        return {parsed.get<Recipe>()};
    }

    throw nlohmann::json::type_error::create(
        302,
        "recipe JSON must be an object or array",
        &parsed);
}

Agent::Agent(long id) : traderId(id) {}

Action Agent::policy(const Observation& observation) {
    return Action();
}

