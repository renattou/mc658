////////////////////////////////////////////////////////////////////////////////
//
//  @file: metaheuristic.hpp
//  @author: Renato Landim Vargas
//  @time: 2017-11-17T05:18:14.072Z
//
//  @brief TODO
//
////////////////////////////////////////////////////////////////////////////////

#include "metaheuristic.hpp"

#include <chrono>

////////////////////////////////////////////////////////////////////////////////
// Constants
const short N_MEMBERS = 25; // Size of population (use odd numbers to crossover all but the fittest)
const float MUTATION_RATE = 0.01f; // Probability of mutating a gene
const float CROSSOVER_MIN_RATE = 0.5f; // Min percentage of genes to be crossed-over
const float CROSSOVER_MAX_RATE = 0.8f; // Max percentage of genes to be crossed-over

////////////////////////////////////////////////////////////////////////////////
// Auxiliary function to calculate the total cost of a solution
int get_cost(solution& sol)
{
  int cost = 0;
  // Calculates cost for each actor
  for (short i = 0; i < nactors; i++) {
    short first_day = 0, last_day = nscenes - 1;
    // Finds first day of work
    for (short j = 0; j < nscenes; j++) {
      if (t[i][sol.sol[j]]) {
        first_day = j;
        break;
      }
    }
    // Finds last day of work
    for (short j = nscenes-1; j >= 0; j--) {
      if (t[i][sol.sol[j]]) {
        last_day = j;
        break;
      }
    }
    // Sums actor cost to total
    cost += (last_day - first_day + 1 - wdays[i]) * costs[i];
  }
  return cost;
}

////////////////////////////////////////////////////////////////////////////////
// Completes solution sol with a greedy approach
void greedy_solution(solution& sol)
{
  // List of pairs (scene, scene cost)
  using scenes_t = std::pair<short, int>;
  std::vector<scenes_t> scenes;
  scenes.reserve(sol.comp.size());

  // Gets only remaining scenes
  for (short scene: sol.comp) {
    scenes.push_back(std::make_pair(scene, scene_costs[scene]));
  }

  // Sorts scenes in decreasing order of scene cost
  std::sort(scenes.begin(), scenes.end(), [](const scenes_t& i, const scenes_t& j) { return i.second > j.second; });

  // Completes solution
  for (auto& scene: scenes) {
    short idx = ++sol.lactive - 1;
    sol.sol[idx] = scene.first;
  }
  sol.comp.clear();

  // Update lower bound
  sol.lower_bound = get_cost(sol);
}

////////////////////////////////////////////////////////////////////////////////
// Completes solution sol with a random approach
void random_solution(solution& sol)
{
  // List of scenes
  std::vector<short> scenes;
  scenes.reserve(sol.comp.size());

  // Gets only remaining scenes
  for (short scene: sol.comp) {
    scenes.push_back(scene);
  }

  // Random shuffle
  std::random_shuffle(scenes.begin(), scenes.end());

  // Completes solution
  for (auto& scene: scenes) {
    short idx = ++sol.lactive - 1;
    sol.sol[idx] = scene;
  }
  sol.comp.clear();

  // Update lower bound
  sol.lower_bound = get_cost(sol);
}

////////////////////////////////////////////////////////////////////////////////
// Gets the fittest individual on a population
solution get_fittest(std::vector<solution>& population, int& total_fitness)
{
  int fittest_idx = 0;
  total_fitness = 0;
  // Search fittest individual of population
  for (short i = 1; i < (short)population.size(); i++) {
    if (population[i].lower_bound <= population[fittest_idx].lower_bound) {
      fittest_idx = i;
    }
    total_fitness += population[i].lower_bound;
  }
  // Checks if fittest individual is also best solution
  solution fittest = population[fittest_idx];
  if (fittest.lower_bound < best_sol.lower_bound) {
    best_sol = fittest;
  }
  return fittest;
}

////////////////////////////////////////////////////////////////////////////////
// Performs one of the possible types of crossover on two "parents"
void crossover(solution& individual_1, solution& individual_2)
{
  // Chooses crossover type
  short crossover_type = rand() % 3;

  // Copies individual 1
  solution temp_individual_1 = individual_1;

  // Crossover range
  short min_range = (short)std::ceil(CROSSOVER_MIN_RATE * nscenes);
  short max_range = (short)std::ceil(CROSSOVER_MAX_RATE * nscenes);
  short range = min_range + rand() % (max_range - min_range + 1);
  if (range == 0) {
    return;
  }
  else if (range == nscenes) {
    individual_1 = individual_2;
    individual_2 = temp_individual_1;
    return;
  }

  // Crossover indexes
  short min, max, idx1, idx2;
  if (crossover_type <= 0) {
    // Does crossover at the beginning
    min = 0;
    max = min + (range - 1);
  }
  else if (crossover_type <= 1) {
    // Does crossover at the end
    max = nscenes - 1;
    min = max - (range - 1);
  }
  else {
    // Does crossover at the middle
    min = (range == nscenes) ? 0 : rand() % (nscenes - range);
    max = min + (range - 1);
  }

  // Keep individual 1 genes on crossover range
  // and copies remaining genes from individual 2 on order
  auto it_min = individual_1.sol.begin() + min;
  auto it_max = individual_1.sol.begin() + max + 1;
  idx2 = 0;
  for (idx1 = 0; idx1 < nscenes - 1; idx1++) {
    // Finds next gene not already present on new individual
    while (std::find(it_min, it_max, individual_2.sol[idx2]) == it_max) {
      idx2++;
    }
    // Replaces genes outside crossover range
    if (idx1 < min && idx1 > max) {
      individual_1.sol[idx1] = individual_2.sol[idx2];
    }
  }

  // Keep individual 2 genes on crossover range
  // and copies remaining genes from individual 1 on order
  it_min = individual_2.sol.begin() + min;
  it_max = individual_2.sol.begin() + max + 1;
  idx1 = 0;
  for (idx2 = 0; idx2 < nscenes - 1; idx2++) {
    // Finds next gene not already present on new individual
    while (std::find(it_min, it_max, temp_individual_1.sol[idx1]) == it_max) {
      idx1++;
    }
    // Replaces genes outside crossover range
    if (idx2 < min && idx2 > max) {
      individual_2.sol[idx2] = temp_individual_1.sol[idx1];
    }
  }

  // Updates fitness
  individual_1.lower_bound = get_cost(individual_1);
  individual_2.lower_bound = get_cost(individual_2);
}

////////////////////////////////////////////////////////////////////////////////
// Performs one of the possible types of mutation on an individual
void mutate(solution& individual)
{
  // Chooses mutation type
  short mutation_type = rand() % 3;

  // Tries mutating every scene by swapping
  short idx2 = 0;
  for (short idx1 = 0; idx1 < nscenes - 1; idx1++) {
    float mutation_chance = (double)rand() / (RAND_MAX);
    if (mutation_chance < MUTATION_RATE) {
      if (mutation_type <= 0) {
        // Stops at half size
        if (idx1 > nscenes / 2) {
          break;
        }
        // Gets opposite gene
        idx2 = nscenes - idx1 - 1;
      }
      else if (mutation_type <= 1) {
        // Gets neighbour gene
        idx2 = idx1 + 1;
      }
      else {
        // Gets random gene after this one
        idx2 = idx1 + 1 + rand() % (nscenes - idx1 - 1);
      }
      // Swaps scenes
      short scene1 = individual.sol[idx1];
      individual.sol[idx1] = individual.sol[idx2];
      individual.sol[idx2] = scene1;
    }
  }

  // Updates fitness
  individual.lower_bound = get_cost(individual);
}

////////////////////////////////////////////////////////////////////////////////
// Chooses on individual by the roulette method
int roulette(std::vector<solution>& population, int total_fitness)
{
  // Randomizes roulette range
  float random_probability = (double)rand() / (RAND_MAX);
  // Searches for individual on this range
  float current_probability = 0;
  for (short i = 0; i < N_MEMBERS; i++) {
    float fitness_ratio = population[i].lower_bound / (float)total_fitness;
    current_probability += (1 - fitness_ratio) / (N_MEMBERS - 1);
    if (current_probability >= random_probability) {
      return i;
    }
  }
  // Should not reach this point
  return rand() % N_MEMBERS;
}

////////////////////////////////////////////////////////////////////////////////
// Evolves a population to the next generation
void evolve_population(std::vector<solution>& population, int total_fitness)
{
  // Creates new population
  std::vector<solution> new_population;
  new_population.reserve(N_MEMBERS);

  // Saves fittest individual
  solution fittest = get_fittest(population, total_fitness);
  new_population.push_back(fittest);

  // Generates new individuals
  for (short i = 1; i < N_MEMBERS; i = i + 2) {
    // Gets parents and creates children
    short parent_idx1 = roulette(population, total_fitness);
    short parent_idx2 = roulette(population, total_fitness);

    solution child_1 = population[parent_idx1];
    solution child_2 = population[parent_idx2];

    // Crossovers parents genes
    crossover(child_1, child_2);

    // Mutates children
    mutate(child_1);
    mutate(child_2);

    // Saves children to new population
    new_population.push_back(child_1);
    new_population.push_back(child_2);
  }

  // Updates population
  population = new_population;
}

////////////////////////////////////////////////////////////////////////////////
// Performs genetic algorithm meta heuristics
void genetic_algorithm(float time_max)
{
  // Creates population
  std::vector<solution> population;
  population.reserve(N_MEMBERS);

  // Runs greedy algorithm for initial best solution
  best_sol = solution(nscenes);
  greedy_solution(best_sol);
  population.push_back(best_sol);

  // Randomizes individuals
  for (short i = 1; i < N_MEMBERS; i++) {
    solution new_sol(nscenes);
    random_solution(new_sol);
    population.push_back(new_sol);
  }

  // Updates best solution
  int total_fitness = 0;
  solution fittest = get_fittest(population, total_fitness);
  std::cout << "Generation 0 -> Fittest: " << fittest.lower_bound << std::endl;

  // Timer initialization
  auto time_start = std::chrono::high_resolution_clock::now();
  auto time_now = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float, std::milli> time_delta = time_start - time_now;

  // Runs until timeout
  int generation = 0;
  while (time_delta.count() < time_max) {
    // Evolves population
    generation++;
    evolve_population(population, total_fitness);

    // Gets fittest solution and total fitness
    solution new_fittest = get_fittest(population, total_fitness);

    // Updates elapsed time
    time_now = std::chrono::high_resolution_clock::now();
    time_delta = time_now - time_start;

    // Prints generation results
    if (new_fittest.lower_bound < fittest.lower_bound || time_delta.count() >= time_max) {
      fittest = new_fittest;
      std::cout << "Generation " << generation;
      std::cout << " -> Fittest: " << fittest.lower_bound;
      std::cout << " / Time: " << time_delta.count() / 1000 << std::endl;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
