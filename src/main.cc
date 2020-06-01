#include <cstdio>
#include <cstdlib>
#include <cassert>

#include <numeric>
#include <algorithm>

#include <vector>
#include <array>
#include <fstream>

#include <imgui.h>
#include <imgui-SFML.h>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable : 4127) // Conditional expression is constant
#endif
#include "attacks.pb.h"
#ifdef _MSC_VER
#	pragma warning(pop)
#endif

#if IMGUI_VERSION_NUM != 17500 && !defined ANDONI_DISABLE_IMGUI_VERSION_CHECK
#	error IMGUI_VERSION_NUM unexpected. This program was built to use ImGui 1.75. Define ANDONI_DISABLE_IMGUI_VERSION_CHECK to disable this error
#endif

void compute_probability_for_hit_count_recursive(
	const int remaining_attempt_count,
	const double current_probability,
	const double success_probability_per_attempt,
	const int current_hit_count,
	std::vector<double>& probability_for_hit_count)
{
	if (remaining_attempt_count == 0)
	{
		assert(current_hit_count >= 0 && current_hit_count < static_cast<int>(probability_for_hit_count.size()));

		probability_for_hit_count[current_hit_count] += current_probability;
		return;
	}

	compute_probability_for_hit_count_recursive(remaining_attempt_count - 1, current_probability * success_probability_per_attempt, success_probability_per_attempt,
		current_hit_count + 1, probability_for_hit_count);
	compute_probability_for_hit_count_recursive(remaining_attempt_count - 1, current_probability * (1.0 - success_probability_per_attempt), success_probability_per_attempt,
		current_hit_count, probability_for_hit_count);
}

struct AttackInputData
{
	int attempt_count;
	double success_probability_per_attempt;
	int max_damage;

	static constexpr double min_probability = 0.0;
	static constexpr double max_probability = 1.0;
	static constexpr int min_attemps = 1;
	static constexpr int min_max_damage = 0;

	void assert_invariants() const noexcept
	{
		assert(attempt_count >= min_attemps);
		assert(success_probability_per_attempt >= min_probability && success_probability_per_attempt <= max_probability);
		assert(max_damage >= min_max_damage);
	}
};

struct AttackComputedData
{
	std::vector<double> probability_per_hit_count;
	int biggest_probability_index = -1;
	double average_damage = 0.0;

	void assert_invariants() const noexcept
	{
		assert(biggest_probability_index != -1 && "Attack's computed data uninitialized");
		assert(!probability_per_hit_count.empty() && "Attack's computed data uninitialized");
		assert(biggest_probability_index >= 0 && biggest_probability_index < static_cast<int>(probability_per_hit_count.size()));
		assert(average_damage >= 0.0);
	}
};

[[nodiscard]] int compute_damage(const int max_damage, const int attempt_count, const int hit_count)
{
	const double damage = static_cast<double>(max_damage) * (static_cast<double>(hit_count) / static_cast<double>(attempt_count));

	// TODO: check which is the correct rounding mode
	return static_cast<int>(std::round(damage));
}

[[nodiscard]] AttackComputedData compute_probabilities(const AttackInputData info)
{
	info.assert_invariants();

	AttackComputedData out;

	out.probability_per_hit_count = std::vector<double>(static_cast<std::size_t>(info.attempt_count + 1), 0.0);
	compute_probability_for_hit_count_recursive(info.attempt_count, 1.0, info.success_probability_per_attempt, 0 /* current_hit_count */, out.probability_per_hit_count);

	const auto most_probable_it = std::max_element(std::cbegin(out.probability_per_hit_count), std::cend(out.probability_per_hit_count));
	assert(most_probable_it != std::cend(out.probability_per_hit_count));

	out.biggest_probability_index = static_cast<int>(most_probable_it - std::cbegin(out.probability_per_hit_count));

	double average_damage = 0.0;
	for (int hit_count = 0; hit_count != info.attempt_count + 1; ++hit_count)
	{
		average_damage += out.probability_per_hit_count[hit_count] * compute_damage(info.max_damage, info.attempt_count, hit_count);
	}
	out.average_damage = average_damage;

	return out;
}

void percentage_progress_bar(const double fraction)
{
	char percentage[64];
#ifdef _MSC_VER
	sprintf_s(
#else
	std::sprintf(
#endif
		percentage, "%.3g%%", fraction * 100.0);

	const ImVec2 default_size{ -1.0f, 0.0f };
	ImGui::ProgressBar(static_cast<float>(fraction), default_size, percentage);
}

void output_probabilities(const AttackInputData& input, const AttackComputedData& computed)
{
	input.assert_invariants();
	computed.assert_invariants();

	const double biggest_probability = computed.probability_per_hit_count[computed.biggest_probability_index];

	for (int hit_count = 0; hit_count != input.attempt_count + 1; ++hit_count)
	{
		const double hit_count_probability = computed.probability_per_hit_count[hit_count];
		const int damage = compute_damage(input.max_damage, input.attempt_count, hit_count);

		ImGui::Text("%2d hit(s): %5.2f%% Damage: %3d", hit_count, hit_count_probability * 100.0, damage);

		ImGui::SameLine();

		const auto probability = computed.probability_per_hit_count[hit_count];
		percentage_progress_bar(probability);
	}

	ImGui::Text("\nAverage damage: %g", computed.average_damage);
}

struct Attack
{
	static constexpr std::size_t name_array_size = 64;

	std::array<char, name_array_size> name{ "New attack" };
	AttackInputData input{ 1, 0.5, 1 };
	AttackComputedData computed;
};

using Attacks = std::vector<Attack>;

struct Global
{
	Attacks attacks;
	int damage_calculator_target_damage = 0;

	static constexpr char confirm_delete_attack_popup_id[] = "Confirm deleting attack";
	static constexpr int no_attack_to_delete = -1;

	int attack_to_delete_index = no_attack_to_delete;
};

[[nodiscard]] proto::Attack to_protobuf(const Attack& attack)
{
	proto::Attack out_attack;
	out_attack.set_name(attack.name.data());
	out_attack.set_attempt_count(attack.input.attempt_count);
	out_attack.set_max_damage(attack.input.max_damage);
	out_attack.set_success_probability_per_attempt(attack.input.success_probability_per_attempt);

	return out_attack;
}

[[nodiscard]] proto::Attacks to_protobuf(const Attacks & attacks)
{
	proto::Attacks out_attacks;
	for (const Attack& attack : attacks)
	{
		attack.input.assert_invariants();
		*out_attacks.add_attacks() = to_protobuf(attack);
	}

	return out_attacks;
}

[[nodiscard]] Attack from_protobuf(const proto::Attack& attack)
{
	Attack out_attack;

	if (const auto& name = attack.name(); !name.empty())
	{
		out_attack.name.fill('\0');
		std::copy_n(name.cbegin(), std::min(name.size(), Attack::name_array_size), std::begin(out_attack.name));
	}

	out_attack.input.attempt_count = std::max(attack.attempt_count(), AttackInputData::min_attemps);
	out_attack.input.max_damage = std::max(attack.max_damage(), AttackInputData::min_max_damage);
	out_attack.input.success_probability_per_attempt = std::clamp(attack.success_probability_per_attempt(), AttackInputData::min_probability, AttackInputData::max_probability);

	out_attack.computed = compute_probabilities(out_attack.input);

	return out_attack;
}

template <typename Derived, typename Base>
constexpr bool is_derived_from = std::is_base_of_v<Base, Derived> && !std::is_same_v<Base, Derived>;

struct FromProtobuf
{
	template <typename Proto, typename = std::enable_if_t<is_derived_from<Proto, google::protobuf::Message>>>
	decltype(auto) operator()(const Proto& proto) noexcept(noexcept(from_protobuf(proto)))
	{
		return from_protobuf(proto);
	}
};

[[nodiscard]] Attacks from_protobuf(const proto::Attacks& attacks)
{
	Attacks out_attacks;
	out_attacks.reserve(static_cast<std::size_t>(attacks.attacks_size()));
	std::transform(attacks.attacks().cbegin(), attacks.attacks().cend(), std::back_inserter(out_attacks), FromProtobuf{});

	return out_attacks;
}

constexpr char attacks_filename[] = "attacks.protobin";
void save_attacks(const Attacks& attacks, const char* filename = attacks_filename)
{
	std::ofstream out_file(filename, std::ios_base::binary);
	if (!out_file.is_open())
		return;

	const proto::Attacks out_attacks = to_protobuf(attacks);
	out_attacks.SerializeToOstream(&out_file);
}

[[nodiscard]] Attacks load_attacks(const char* filename = attacks_filename)
{
	std::ifstream in_file(filename, std::ios_base::binary);
	if (!in_file.is_open())
		return {};

	proto::Attacks attacks;
	if (!attacks.ParseFromIstream(&in_file))
		return {};

	return from_protobuf(attacks);
}

void update(Global& global)
{
#ifdef ANDONI_DEBUG
	ImGui::ShowDemoWindow();
#endif

	if (ImGui::Begin("Attacks"))
	{
		if (ImGui::BeginPopupModal(Global::confirm_delete_attack_popup_id))
		{
			assert(global.attack_to_delete_index != Global::no_attack_to_delete && "Logic error");
			assert(global.attack_to_delete_index >= 0 && global.attack_to_delete_index < global.attacks.size());
			ImGui::Text("Do you want to delete the attack \"%s\"?", global.attacks[global.attack_to_delete_index].name.data());
			if (ImGui::Button("Yes") || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_::ImGuiKey_Enter), false))
			{
				global.attacks.erase(global.attacks.begin() + global.attack_to_delete_index);
				global.attack_to_delete_index = Global::no_attack_to_delete;

				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No"))
			{
				global.attack_to_delete_index = Global::no_attack_to_delete;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		const auto attack_count = static_cast<int>(global.attacks.size());
		for (int i = 0; i < attack_count; ++i)
		{
			Attack& attack = global.attacks[i];
			bool attack_is_open = true;
			if (ImGui::CollapsingHeader((std::string(attack.name.data()) + "###" + std::to_string((std::uintptr_t)(&attack))).c_str(), &attack_is_open))
			{
				ImGui::PushID(&attack);

				bool attack_changed = false;

				{
					std::array<char, Attack::name_array_size> name_copy = attack.name;
					if (ImGui::InputText("Name", name_copy.data(), name_copy.size()))
					{
						attack.name = name_copy;
						attack_changed = true;
					}
				}

				bool need_to_recompute = false;

				if (ImGui::InputInt("Attempt count", &attack.input.attempt_count))
				{
					attack.input.attempt_count = std::max(attack.input.attempt_count, AttackInputData::min_attemps);
					need_to_recompute = true;
					attack_changed = true;
				}

				{
					int success_probability = static_cast<int>(attack.input.success_probability_per_attempt * 100.0);
					if (ImGui::InputInt("Success probability", &success_probability))
					{
						attack.input.success_probability_per_attempt = std::clamp(static_cast<double>(success_probability) / 100.0, AttackInputData::min_probability, AttackInputData::max_probability);
						need_to_recompute = true;
						attack_changed = true;
					}
				}

				if (ImGui::InputInt("Max damage", &attack.input.max_damage))
				{
					attack.input.max_damage = std::max(attack.input.max_damage, AttackInputData::min_max_damage);
					need_to_recompute = true;
					attack_changed = true;
				}

				if (need_to_recompute)
					attack.computed = compute_probabilities(attack.input);

				if (attack_changed)
					save_attacks(global.attacks);

				if (ImGui::TreeNode("Stats"))
				{
					output_probabilities(attack.input, attack.computed);

					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			if (!attack_is_open)
			{
				ImGui::OpenPopup(Global::confirm_delete_attack_popup_id);

				assert(global.attack_to_delete_index == Global::no_attack_to_delete && "Logic error");
				global.attack_to_delete_index = i;
			}
		}

		if (ImGui::Button("Add attack"))
		{
			Attack& new_attack = global.attacks.emplace_back();
			new_attack.computed = compute_probabilities(new_attack.input);
		}
	}
	ImGui::End();

	if (ImGui::Begin("Damage"))
	{
		if (ImGui::InputInt("Target damage", &global.damage_calculator_target_damage))
			global.damage_calculator_target_damage = std::max(global.damage_calculator_target_damage, 0);

		std::vector<double> probability_to_reach_target_per_attack;
		probability_to_reach_target_per_attack.reserve(global.attacks.size());

		for (const Attack& attack : global.attacks)
		{
			attack.input.assert_invariants();
			attack.computed.assert_invariants();

			double probability_to_reach_target = 0.0;
			for (int hit_count = 0; hit_count != attack.input.attempt_count + 1; ++hit_count)
			{
				const auto damage = compute_damage(attack.input.max_damage, attack.input.attempt_count, hit_count);
				if (damage >= global.damage_calculator_target_damage)
					probability_to_reach_target += attack.computed.probability_per_hit_count[hit_count];
			}

			probability_to_reach_target_per_attack.push_back(probability_to_reach_target);
		}

		std::vector<int> attack_indices;
		attack_indices.resize(global.attacks.size());
		std::iota(attack_indices.begin(), attack_indices.end(), 0);

		std::sort(attack_indices.begin(), attack_indices.end(), [&probability_to_reach_target_per_attack](const int left_index, const int right_index) noexcept
		{
			const double left_attack_success_probability = probability_to_reach_target_per_attack[left_index];
			const double right_attack_success_probability = probability_to_reach_target_per_attack[right_index];

			return left_attack_success_probability > right_attack_success_probability;
		});

		const auto attack_count = static_cast<int>(global.attacks.size());
		for (const int attack_index : attack_indices)
		{
			const double probability_to_reach_target = probability_to_reach_target_per_attack[attack_index];
			const Attack& attack = global.attacks[attack_index];

			ImGui::TextUnformatted(attack.name.data());
			ImGui::SameLine();

			percentage_progress_bar(probability_to_reach_target);
		}
	}
	ImGui::End();
}

int main()
{
	Global global;
	global.attacks = load_attacks();

	sf::RenderWindow window(sf::VideoMode(1280, 720), "For the King calculator");
	window.setFramerateLimit(60);
	ImGui::SFML::Init(window);

	sf::Clock delta_time_clock;
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			ImGui::SFML::ProcessEvent(event);

			if (event.type == sf::Event::Closed)
			{
				window.close();
			}
		}

		ImGui::SFML::Update(window, delta_time_clock.restart());

		update(global);

		window.clear();
		ImGui::SFML::Render(window);
		window.display();
	}

	save_attacks(global.attacks);

	ImGui::SFML::Shutdown();
}
