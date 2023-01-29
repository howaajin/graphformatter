/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "graph_layout.h"

#include <limits>
#include <memory>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <iostream>
#include <fstream>
#include <queue>

namespace graph_layout
{
    using namespace std;

    struct fas_positioning_strategy_t
    {
        vector<vector<node_t*>>& layers;
        bool is_horizontal_dir;
        vector<rect_t> layers_bound;
        vector2_t spacing{80, 80};
        bool is_upper_dir = true;
        bool is_left_dir = true;
        map<node_t*, node_t*> conflict_marks{};
        map<node_t*, node_t*> root_map{};
        map<node_t*, node_t*> align_map{};
        map<node_t*, node_t*> sink_map{};
        map<node_t*, float> shift_map{};
        map<node_t*, float> inner_shift_map{};
        map<node_t*, float>* x_map{};
        map<node_t*, size_t> index_map{};
        map<node_t*, float> block_width_map{};
        map<node_t*, node_t*> predecessor_map{};
        map<node_t*, node_t*> successor_map{};
        map<node_t*, float> upper_left_pos_map{};
        map<node_t*, float> upper_right_pos_map{};
        map<node_t*, float> lower_left_pos_map{};
        map<node_t*, float> lower_right_pos_map{};
        map<node_t*, float> combined_pos_map{};
        rect_t assign_coordinate();

    private:
        void initialize();
        void mark_conflicts();
        void do_vertical_alignment();
        void calculate_inner_shift();
        void place_block(node_t* block_root);
        void compact();
        void one_pass();
        void combine();
    };

    rect_t fas_positioning_strategy_t::assign_coordinate()
    {
        node_t* first_node = layers[0][0];
        const vector2_t old_position = first_node->position;

        initialize();
        is_upper_dir = true;
        is_left_dir = true;
        x_map = &upper_left_pos_map;
        one_pass();

        is_upper_dir = true;
        is_left_dir = false;
        x_map = &upper_right_pos_map;
        one_pass();

        is_upper_dir = false;
        is_left_dir = true;
        x_map = &lower_left_pos_map;
        one_pass();

        is_upper_dir = false;
        is_left_dir = false;
        x_map = &lower_right_pos_map;
        one_pass();

        combine();

        for (int i = 0; i < layers.size(); i++)
        {
            auto& layer = layers[i];
            for (auto node : layer)
            {
                float x, y;
                float *p_x, *p_y;
                if (is_horizontal_dir)
                {
                    p_x = &x;
                    p_y = &y;
                }
                else
                {
                    p_x = &y;
                    p_y = &x;
                }
                if (node->in_edges.empty())
                {
                    if (is_horizontal_dir)
                    {
                        *p_x = layers_bound[i].r - node->size.x;
                    }
                    else
                    {
                        *p_x = layers_bound[i].b - node->size.y;
                    }
                }
                else
                {
                    if (is_horizontal_dir)
                    {
                        *p_x = layers_bound[i].l;
                    }
                    else
                    {
                        *p_x = layers_bound[i].t;
                    }
                }
                *p_y = (*x_map)[node];
                node->set_position(vector2_t{x, y});
            }
        }
        const vector2_t position = first_node->position;
        const vector2_t offset = old_position - position;
        const auto& bound_pos = first_node->position;
        rect_t bound{bound_pos.x, bound_pos.y, bound_pos.x, bound_pos.y};
        for (auto& layer : layers)
        {
            for (auto node : layer)
            {
                node->set_position(node->position + offset);
                bound = bound.expand(node->position, node->size);
            }
        }
        return bound;
    }

    void fas_positioning_strategy_t::initialize()
    {
        for (auto& layer : layers)
        {
            for (size_t i = 0; i < layer.size(); i++)
            {
                auto node = layer[i];
                index_map[node] = i;
                predecessor_map[node] = i == 0 ? nullptr : layer[i - 1];
                successor_map[node] = i == layer.size() - 1 ? nullptr : layer[i + 1];
            }
        }
        mark_conflicts();
    }

    void fas_positioning_strategy_t::mark_conflicts()
    {
        for (int i = 1; i < layers.size() - 1; i++)
        {
            int k0 = 0;
            int l = 1;
            for (int l1 = 0; l1 < layers[i + 1].size(); l1++)
            {
                auto node = layers[i + 1][l1];
                bool is_crossing_inner_segment = node->is_crossing_inner_segment(layers[i + 1], layers[i]);
                if (l1 == layers[i + 1].size() - 1 || is_crossing_inner_segment)
                {
                    int k1 = layers[i].size();
                    if (is_crossing_inner_segment)
                    {
                        const auto median_upper = node->get_median_upper();
                        k1 = index_map[median_upper];
                    }
                    while (l < l1)
                    {
                        auto upper_nodes = node->get_uppers();
                        for (auto upper_node : upper_nodes)
                        {
                            auto k = index_map[upper_node];
                            if (k < k0 || k > k1)
                            {
                                conflict_marks.insert(make_pair(upper_node, node));
                            }
                        }
                        ++l;
                    }
                    k0 = k1;
                }
            }
        }
    }

    void fas_positioning_strategy_t::do_vertical_alignment()
    {
        root_map.clear();
        align_map.clear();
        for (const auto& layer : layers)
        {
            for (auto node : layer)
            {
                root_map.insert(make_pair(node, node));
                align_map.insert(make_pair(node, node));
            }
        }
        int layer_step = is_upper_dir ? 1 : -1;
        int layer_start = is_upper_dir ? 0 : layers.size() - 1;
        int layer_end = is_upper_dir ? layers.size() : -1;
        for (int i = layer_start; i != layer_end; i += layer_step)
        {
            int guide = is_left_dir ? -1 : INT_MAX;
            int step = is_left_dir ? 1 : -1;
            int start = is_left_dir ? 0 : layers[i].size() - 1;
            int end = is_left_dir ? layers[i].size() : -1;
            for (int k = start; k != end; k += step)
            {
                auto node = layers[i][k];
                auto adjacencies = is_upper_dir ? node->get_uppers() : node->get_lowers();
                if (!adjacencies.empty())
                {
                    int ma = trunc((adjacencies.size() + 1) / 2.0f - 1);
                    int mb = ceil((adjacencies.size() + 1) / 2.0f - 1);
                    for (int m = ma; m <= mb; m++)
                    {
                        if (align_map[node] == node)
                        {
                            auto& median_node = adjacencies[m];
                            bool is_marked = conflict_marks.find(median_node) != conflict_marks.end() && conflict_marks[median_node] == node;
                            float max_weight = median_node->get_max_weight(!is_upper_dir);
                            float link_weight = node->get_max_weight_to_node(median_node, is_upper_dir);
                            const auto median_node_pos = index_map[median_node];
                            bool guide_accepted = is_left_dir ? median_node_pos > guide : median_node_pos < guide;
                            if (!is_marked)
                            {
                                if (guide_accepted && link_weight == max_weight)
                                {
                                    align_map[median_node] = node;
                                    root_map[node] = root_map[median_node];
                                    align_map[node] = root_map[node];
                                    guide = median_node_pos;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void fas_positioning_strategy_t::calculate_inner_shift()
    {
        inner_shift_map.clear();
        block_width_map.clear();
        for (auto& layer : layers)
        {
            for (auto node : layer)
            {
                if (root_map[node] == node)
                {
                    inner_shift_map.insert(make_pair(node, 0.0f));
                    float left = 0, right = is_horizontal_dir ? node->size.y : node->size.x;
                    auto root_node = node;
                    auto upper_node = node;
                    auto lower_node = align_map[root_node];
                    while (true)
                    {
                        const float upper_position = upper_node->get_linked_position_to_node(lower_node, !is_upper_dir, is_horizontal_dir);
                        const float lower_position = lower_node->get_linked_position_to_node(upper_node, is_upper_dir, is_horizontal_dir);
                        const float shift = inner_shift_map[upper_node] + upper_position - lower_position;
                        inner_shift_map[lower_node] = shift;
                        left = std::min(left, shift);
                        right = std::max(right, shift + (is_horizontal_dir ? lower_node->size.y : lower_node->size.x));
                        upper_node = lower_node;
                        lower_node = align_map[upper_node];
                        if (lower_node == root_node)
                        {
                            break;
                        }
                    }
                    auto check_node = node;
                    do
                    {
                        inner_shift_map[check_node] -= left;
                        check_node = align_map[check_node];
                    }
                    while (check_node != node);
                    block_width_map[node] = right - left;
                }
            }
        }
    }

    void fas_positioning_strategy_t::place_block(node_t* block_root)
    {
        if (std::isnan((*x_map)[block_root]))
        {
            bool initial = true;
            (*x_map)[block_root] = 0;
            auto node = block_root;
            do
            {
                const auto adjacency = is_left_dir ? predecessor_map[node] : successor_map[node];
                if (adjacency != nullptr)
                {
                    float adjacency_height, node_height;
                    float spacing1;
                    if (is_horizontal_dir)
                    {
                        adjacency_height = adjacency->size.y;
                        node_height = node->size.y;
                        spacing1 = spacing.y;
                    }
                    else
                    {
                        adjacency_height = adjacency->size.x;
                        node_height = node->size.x;
                        spacing1 = spacing.x;
                    }

                    const auto prev_block_root = root_map[adjacency];
                    place_block(prev_block_root);
                    if (sink_map[block_root] == block_root)
                    {
                        sink_map[block_root] = sink_map[prev_block_root];
                    }
                    if (sink_map[block_root] != sink_map[prev_block_root])
                    {
                        float left_shift = (*x_map)[block_root] - (*x_map)[prev_block_root] + inner_shift_map[node] - inner_shift_map[adjacency] - adjacency_height - spacing1;
                        float right_shift = (*x_map)[block_root] - (*x_map)[prev_block_root] - inner_shift_map[node] + inner_shift_map[adjacency] + node_height + spacing1;
                        float shift = is_left_dir ? std::min(shift_map[sink_map[prev_block_root]], left_shift) : std::max(shift_map[sink_map[prev_block_root]], right_shift);
                        shift_map[sink_map[prev_block_root]] = shift;
                    }
                    else
                    {
                        float left_shift = inner_shift_map[adjacency] + adjacency_height - inner_shift_map[node] + spacing1;
                        float right_shift = -node_height - spacing1 + inner_shift_map[adjacency] - inner_shift_map[node];
                        float shift = is_left_dir ? left_shift : right_shift;
                        float position = (*x_map)[prev_block_root] + shift;
                        if (initial)
                        {
                            (*x_map)[block_root] = position;
                            initial = false;
                        }
                        else
                        {
                            position = is_left_dir ? std::max((*x_map)[block_root], position) : std::min((*x_map)[block_root], position);
                            (*x_map)[block_root] = position;
                        }
                    }
                }
                node = align_map[node];
            }
            while (node != block_root);
        }
    }

    void fas_positioning_strategy_t::compact()
    {
        sink_map.clear();
        shift_map.clear();
        x_map->clear();
        for (auto& layer : layers)
        {
            for (auto node : layer)
            {
                sink_map.insert(make_pair(node, node));
                if (is_left_dir)
                {
                    inner_shift_map.insert(make_pair(node, FLT_MAX));
                }
                else
                {
                    shift_map.insert(make_pair(node, -FLT_MAX));
                }
                x_map->insert(make_pair(node, NAN));
            }
        }
        for (auto& layer : layers)
        {
            for (auto node : layer)
            {
                if (root_map[node] == node)
                {
                    place_block(node);
                }
            }
        }
        for (auto& layer : layers)
        {
            for (auto node : layer)
            {
                auto& root_node = root_map[node];
                (*x_map)[node] = (*x_map)[root_node];
            }
        }
        for (auto& layer : layers)
        {
            for (auto node : layer)
            {
                auto& root_node = root_map[node];
                const float shift = shift_map[sink_map[root_node]];
                if ((is_left_dir && shift < FLT_MAX) || (!is_left_dir && shift > -FLT_MAX))
                {
                    (*x_map)[node] = (*x_map)[node] + shift;
                }
            }
        }
        for (auto& layer : layers)
        {
            for (auto node : layer)
            {
                (*x_map)[node] += inner_shift_map[node];
            }
        }
    }

    void fas_positioning_strategy_t::one_pass()
    {
        do_vertical_alignment();
        calculate_inner_shift();
        compact();
    }

    void fas_positioning_strategy_t::combine()
    {
        vector layouts = {upper_left_pos_map, upper_right_pos_map, lower_left_pos_map, lower_right_pos_map};
        vector<tuple<float, float>> bounds(layouts.size());
        int min_width_index = -1;
        float min_width = FLT_MAX;
        for (int i = 0; i < layouts.size(); i++)
        {
            auto& layout = layouts[i];
            float left_most = FLT_MAX, right_most = -FLT_MAX;
            for (auto& pair : layout)
            {
                if (pair.second < left_most)
                {
                    left_most = pair.second;
                }
                if (pair.second > right_most)
                {
                    right_most = pair.second;
                }
            }
            if (right_most - left_most < min_width)
            {
                min_width = right_most - left_most;
                min_width_index = i;
            }
            bounds[i] = tuple(left_most, right_most);
        }
        for (int i = 0; i < layouts.size(); i++)
        {
            if (i != min_width_index)
            {
                float offset = std::get<0>(bounds[min_width_index]) - std::get<0>(bounds[i]);
                for (auto& pair : layouts[i])
                {
                    pair.second += offset;
                }
            }
        }
        for (auto& layer : layers)
        {
            for (auto node : layer)
            {
                vector values = {layouts[0][node], layouts[1][node], layouts[2][node], layouts[3][node]};
                combined_pos_map[node] = (values[2] + values[3]) / 2.0f;
            }
        }
        x_map = &combined_pos_map;
    }

    static void dfs(const node_t* node, set<node_t*>& visited_set, const function<void(node_t*)>& on_visit, const function<void(edge_t*)>& on_non_tree_edge_found)
    {
        for (auto e : node->out_edges)
        {
            auto n = e->head->owner;
            if (visited_set.find(n) == visited_set.end())
            {
                visited_set.insert(n);
                if (on_visit) on_visit(n);
                dfs(n, visited_set, on_visit, on_non_tree_edge_found);
            }
            else
            {
                if (on_non_tree_edge_found)on_non_tree_edge_found(e);
            }
        }
    }

    static void dfs_inv(const node_t* node, set<node_t*>& visited_set, const function<void(node_t*)>& on_visit, const function<void(edge_t*)>& on_non_tree_edge_found)
    {
        for (auto e : node->in_edges)
        {
            auto n = e->tail->owner;
            if (visited_set.find(n) == visited_set.end())
            {
                visited_set.insert(n);
                if (on_visit) on_visit(n);
                dfs_inv(n, visited_set, on_visit, on_non_tree_edge_found);
            }
            else
            {
                if (on_non_tree_edge_found)on_non_tree_edge_found(e);
            }
        }
    }

    int edge_t::length() const
    {
        return head->owner->rank - tail->owner->rank;
    }

    int edge_t::slack() const
    {
        return length() - min_length;
    }

    bool edge_t::is_crossing(const edge_t* other) const
    {
        return (tail->index_in_layer < other->tail->index_in_layer && head->index_in_layer > other->head->index_in_layer) ||
            (tail->index_in_layer > other->tail->index_in_layer && head->index_in_layer < other->head->index_in_layer);
    }

    bool edge_t::is_inner_segment() const
    {
        return tail->owner->is_dummy_node && head->owner->is_dummy_node;
    }

    bool node_t::is_descendant_of(node_t* node) const
    {
        set<node_t*> visited_set;
        vector stack{node};
        while (!stack.empty())
        {
            auto n = stack.back();
            visited_set.insert(n);
            if (n == this)
            {
                return true;
            }
            stack.pop_back();
            for (auto e : n->out_edges)
            {
                if (visited_set.find(e->head->owner) == visited_set.end())
                {
                    stack.push_back(e->head->owner);
                }
            }
        }
        return false;
    }

    void node_t::set_position(vector2_t p)
    {
        vector2_t offset = p - position;
        position = p;
        if (graph)
        {
            graph->translate(offset);
        }
    }

    pin_t* node_t::add_pin(pin_type_t type)
    {
        auto pin = new pin_t{type};
        pin->owner = this;
        (type == pin_type_t::in ? in_pins : out_pins).push_back(pin);
        return pin;
    }

    vector<edge_t*> node_t::get_edges_linked_to_layer(const std::vector<node_t*>& layer, bool is_in) const
    {
        vector<edge_t*> result;
        auto edges = is_in ? in_edges : out_edges;
        for (auto e : edges)
        {
            for (auto layer_node : layer)
            {
                auto n = is_in ? e->tail->owner : e->head->owner;
                if (n == layer_node)
                {
                    result.push_back(e);
                }
            }
        }
        return result;
    }

    bool node_t::is_crossing_inner_segment(const vector<node_t*>& lower_layer, const vector<node_t*>& upper_layer) const
    {
        auto edges_linked_to_upper = get_edges_linked_to_layer(upper_layer, true);
        auto edges_between_layers = connected_graph_t::get_edges_between_two_layers(lower_layer, upper_layer, this);
        for (auto edge_linked_to_upper : edges_linked_to_upper)
        {
            for (auto edge_between_layers : edges_between_layers)
            {
                if (edge_between_layers->is_inner_segment() && edge_linked_to_upper->is_crossing(edge_between_layers))
                {
                    return true;
                }
            }
        }
        return false;
    }

    float node_t::get_barycenter_in_layer(const std::vector<node_t*>& layer, bool is_in) const
    {
        auto edges = get_edges_linked_to_layer(layer, is_in);
        if (edges.empty())
        {
            return -1.0f;
        }
        float sum = 0.0f;
        for (auto e : edges)
        {
            sum += static_cast<float>(is_in ? e->tail->index_in_layer : e->head->index_in_layer);
        }
        return sum / static_cast<float>(edges.size());
    }

    set<node_t*> node_t::get_direct_connected_nodes(const function<bool(edge_t*)>& filter) const
    {
        set<node_t*> result;
        for (auto e : in_edges)
        {
            if (filter(e))
            {
                result.insert(e->tail->owner);
            }
        }
        for (auto e : out_edges)
        {
            if (filter(e))
            {
                result.insert(e->head->owner);
            }
        }
        return result;
    }

    std::set<node_t*> node_t::get_out_nodes() const
    {
        set<node_t*> out_nodes;
        for (auto e : out_edges)
        {
            out_nodes.insert(e->head->owner);
        }
        return out_nodes;
    }

    std::set<node_t*> node_t::get_in_nodes() const
    {
        set<node_t*> in_nodes;
        for (auto e : in_edges)
        {
            in_nodes.insert(e->head->owner);
        }
        return in_nodes;
    }

    node_t* node_t::get_median_upper() const
    {
        vector<node_t*> upper_nodes;
        for (auto edge : in_edges)
        {
            auto it = find(upper_nodes.begin(), upper_nodes.end(), edge->tail->owner);
            if (it == upper_nodes.end())
            {
                upper_nodes.push_back(edge->tail->owner);
            }
        }
        if (!upper_nodes.empty())
        {
            int index = upper_nodes.size() / 2;
            return upper_nodes[index];
        }
        return nullptr;
    }

    vector<node_t*> node_t::get_uppers() const
    {
        set<node_t*> upper_nodes;
        for (auto edge : in_edges)
        {
            upper_nodes.insert(edge->tail->owner);
        }
        return vector(upper_nodes.begin(), upper_nodes.end());
    }

    vector<node_t*> node_t::get_lowers() const
    {
        set<node_t*> lower_nodes;
        for (auto edge : out_edges)
        {
            lower_nodes.insert(edge->head->owner);
        }
        return vector(lower_nodes.begin(), lower_nodes.end());
    }

    float node_t::get_max_weight(bool is_in) const
    {
        auto& edges = is_in ? in_edges : out_edges;
        float max_weight = -FLT_MAX;
        for (auto e : edges)
        {
            if (max_weight < e->weight)
            {
                max_weight = e->weight;
            }
        }
        return max_weight;
    }

    float node_t::get_max_weight_to_node(const node_t* node, bool is_in) const
    {
        auto& edges = is_in ? in_edges : out_edges;
        float max_weight = -FLT_MAX;
        for (auto e : edges)
        {
            auto node_to_check = is_in ? e->tail->owner : e->head->owner;
            if (node_to_check == node && max_weight < e->weight)
            {
                max_weight = e->weight;
            }
        }
        return max_weight;
    }

    float node_t::get_linked_position_to_node(const node_t* node, bool is_in, bool is_horizontal_dir) const
    {
        auto& edges = is_in ? in_edges : out_edges;
        float median_position = 0.0f;
        int count = 0;
        for (auto Edge : edges)
        {
            auto pin = is_in ? Edge->head : Edge->tail;
            auto pin_to_check = is_in ? Edge->tail : Edge->head;
            if (pin_to_check->owner == node)
            {
                if (is_horizontal_dir)
                {
                    median_position += pin->offset.y;
                }
                else
                {
                    median_position += pin->offset.x;
                }
                ++count;
            }
        }
        if (count == 0)
        {
            return 0.0f;
        }
        return median_position / count;
    }

    void node_t::update_pins_offset()
    {
        if (graph)
        {
            auto pins_offset = graph->get_pins_offset();
            for (auto pin : in_pins)
            {
                auto it = pins_offset.find(pin->copy_from);
                if (it != pins_offset.end())
                {
                    auto border = vector2_t{graph->border.l, graph->border.t};
                    pin->offset = it->second + border;
                }
            }
            for (auto pin : out_pins)
            {
                auto it = pins_offset.find(pin->copy_from);
                if (it != pins_offset.end())
                {
                    auto border = vector2_t{graph->border.l, graph->border.t};
                    pin->offset = it->second + border;
                }
            }
            auto comparer = [](const pin_t* a, const pin_t* b)
            {
                return a->offset.y < b->offset.y;
            };
            sort(in_pins.begin(), in_pins.end(), comparer);
            sort(out_pins.begin(), out_pins.end(), comparer);
        }
    }

    void node_t::set_sub_graph(connected_graph_t* g)
    {
        graph = g;
        if (graph)
        {
            auto pins = graph->get_pins();
            for (auto p : pins)
            {
                pin_t* pin = add_pin(p->type);
                pin->copy_from = p;
            }
        }
    }

    node_t::~node_t()
    {
        for (auto pin : in_pins)
        {
            delete pin;
        }
        for (auto pin : out_pins)
        {
            delete pin;
        }
        delete graph;
    }

    node_t* node_t::clone() const
    {
        auto new_node = new node_t();
        new_node->name = name;
        if (graph)
        {
            new_node->graph = graph->clone();
        }
        new_node->position = position;
        for (auto pin : in_pins)
        {
            auto new_pin = new pin_t();
            new_pin->type = pin->type;
            new_pin->offset = pin->offset;
            new_pin->owner = new_node;
            new_node->in_pins.push_back(new_pin);
        }
        for (auto pin : out_pins)
        {
            auto new_pin = new pin_t();
            new_pin->type = pin->type;
            new_pin->offset = pin->offset;
            new_pin->owner = new_node;
            new_node->out_pins.push_back(new_pin);
        }
        return new_node;
    }

    graph_t* connected_graph_t::clone() const
    {
        map<node_t*, node_t*> nodes_map;
        map<pin_t*, pin_t*> pins_map;
        map<edge_t*, edge_t*> edges_map;
        map<node_t*, node_t*> nodes_map_inv;
        map<pin_t*, pin_t*> pins_map_inv;
        map<edge_t*, edge_t*> edges_map_inv;
        return clone(nodes_map, pins_map, edges_map, nodes_map_inv, pins_map_inv, edges_map_inv);
    }

    connected_graph_t* connected_graph_t::clone(std::map<node_t*, node_t*>& nodes_map, std::map<pin_t*, pin_t*>& pins_map, std::map<edge_t*, edge_t*>& edges_map,
                                                std::map<node_t*, node_t*>& nodes_map_inv, std::map<pin_t*, pin_t*>& pins_map_inv, std::map<edge_t*, edge_t*>& edges_map_inv) const
    {
        auto cloned = new connected_graph_t;
        cloned->bound = bound;
        for (auto n : nodes)
        {
            auto cloned_node = n->clone();
            nodes_map[cloned_node] = n;
            nodes_map_inv[n] = cloned_node;
            for (size_t i = 0; i < n->in_pins.size(); i++)
            {
                pins_map_inv[n->in_pins[i]] = cloned_node->in_pins[i];
                pins_map[cloned_node->in_pins[i]] = n->in_pins[i];
            }
            for (size_t i = 0; i < n->out_pins.size(); i++)
            {
                pins_map_inv[n->out_pins[i]] = cloned_node->out_pins[i];
                pins_map[cloned_node->out_pins[i]] = n->out_pins[i];
            }
            cloned->nodes.push_back(cloned_node);
            if (cloned_node->graph)
            {
                cloned->sub_graphs.insert(make_pair(cloned_node, cloned_node->graph));
            }
        }
        for (auto e : edges)
        {
            auto tail_pin = pins_map_inv[e.second->tail];
            auto head_pin = pins_map_inv[e.second->head];
            auto edge = cloned->add_edge(tail_pin, head_pin);
            edges_map[edge] = e.second;
            edges_map_inv[e.second] = edge;
        }
        return cloned;
    }

    graph_t* graph_t::clone() const
    {
        return nullptr;
    }

    node_t* graph_t::add_node(graph_t* sub_graph)
    {
        auto node = new node_t();
        node->graph = sub_graph;
        if (sub_graph)
        {
            sub_graphs.insert(make_pair(node, sub_graph));
        }
        nodes.push_back(node);
        return node;
    }

    node_t* graph_t::add_node(const string& name, graph_t* sub_graph)
    {
        auto node = add_node(sub_graph);
        node->name = name;
        return node;
    }

    void connected_graph_t::set_node_in_rank_slot(node_t* node, rank_slot_t rank_slot)
    {
        switch (rank_slot)
        {
        case rank_slot_t::min:
            {
                min_ranking_node = node;
                pin_t* dummy_pin_out = min_ranking_node->add_pin(pin_type_t::out);
                auto source_nodes = get_source_nodes();
                for (auto n : source_nodes)
                {
                    if (n == node)
                    {
                        continue;
                    }
                    pin_t* dummy_pin_in = n->add_pin(pin_type_t::in);
                    edge_t* dummy_edge = add_edge(dummy_pin_out, dummy_pin_in);
                }
            }
            break;
        case rank_slot_t::max:
            {
                max_ranking_node = node;
                pin_t* dummy_pin_in = max_ranking_node->add_pin(pin_type_t::in);
                auto sink_nodes = get_sink_nodes();
                for (auto n : sink_nodes)
                {
                    if (n == node)
                    {
                        continue;
                    }
                    pin_t* dummy_pin_out = n->add_pin(pin_type_t::out);
                    edge_t* dummy_edge = add_edge(dummy_pin_out, dummy_pin_in);
                }
            }
            break;
        }
    }

    void graph_t::remove_node(node_t* node)
    {
        if (node->graph)
        {
            sub_graphs.erase(node);
        }
        delete node;
    }

    edge_t* graph_t::add_edge(pin_t* tail, pin_t* head)
    {
        auto k = make_pair(tail, head);
        auto it = edges.find(k);
        if (it != edges.end())
        {
            return it->second;
        }
        auto edge = new edge_t{tail, head};
        edges.insert(make_pair(make_pair(tail, head), edge));
        tail->owner->out_edges.push_back(edge);
        head->owner->in_edges.push_back(edge);
        return edge;
    }

    void graph_t::remove_edge(const edge_t* edge)
    {
        auto key = make_pair(edge->tail, edge->head);
        edges.erase(key);
        auto& out_edges = edge->tail->owner->out_edges;
        auto& in_edges = edge->head->owner->in_edges;
        out_edges.erase(find(out_edges.begin(), out_edges.end(), edge));
        in_edges.erase(find(in_edges.begin(), in_edges.end(), edge));
        delete edge;
    }

    void graph_t::remove_edge(pin_t* tail, pin_t* head)
    {
        auto key = make_pair(tail, head);
        auto it = edges.find(key);
        if (it != edges.end())
        {
            remove_edge(it->second);
        }
    }

    void graph_t::invert_edge(edge_t* edge) const
    {
        pin_t* tail = edge->tail;
        pin_t* head = edge->head;
        node_t* tail_node = tail->owner;
        node_t* head_node = head->owner;
        auto& out_edges = tail_node->out_edges;
        auto& in_edges = head_node->in_edges;
        out_edges.erase(find(out_edges.begin(), out_edges.end(), edge));
        in_edges.erase(find(in_edges.begin(), in_edges.end(), edge));
        tail->type = pin_type_t::in;
        head->type = pin_type_t::out;
        tail_node->in_edges.push_back(edge);
        head_node->out_edges.push_back(edge);
        swap(edge->tail, edge->head);
        edge->is_inverted = true;
    }

    std::vector<std::set<node_t*>> graph_t::to_connected_groups() const
    {
        vector<set<node_t*>> result;
        set<node_t*> checked_nodes;
        vector<node_t*> stack;
        for (auto node : nodes)
        {
            if (checked_nodes.find(node) == checked_nodes.end())
            {
                checked_nodes.insert(node);
                stack.push_back(node);
            }
            set<node_t*> isolated_nodes;
            while (!stack.empty())
            {
                node_t* n = stack.back();
                stack.pop_back();
                isolated_nodes.insert(n);
                set<node_t*> connected_nodes = n->get_direct_connected_nodes([](edge_t* e) { return true; });
                for (auto ConnectedNode : connected_nodes)
                {
                    if (checked_nodes.find(node) == checked_nodes.end())
                    {
                        stack.push_back(ConnectedNode);
                        checked_nodes.insert(ConnectedNode);
                    }
                }
            }
            if (!isolated_nodes.empty())
            {
                result.push_back(isolated_nodes);
            }
        }
        return result;
    }

    graph_t* graph_t::to_connected_or_disconnected() const
    {
        auto groups = to_connected_groups();
        if (groups.size() == 1)
        {
            return to_connected(groups[0]);
        }
        else
        {
            auto disconnected_graph = new disconnected_graph_t;
            for (const auto& group : groups)
            {
                disconnected_graph->add_graph(to_connected(group));
            }
            return disconnected_graph;
        }
    }

    graph_t* graph_t::to_connected(const std::set<node_t*>& nodes)
    {
        map<pin_t*, pin_t*> m;
        auto graph = new connected_graph_t;
        for (auto n : nodes)
        {
            auto node = graph->add_node(n->graph);
            node->size = n->size;
            node->user_ptr = n->user_ptr;
            for (auto p : n->in_pins)
            {
                auto pin = node->add_pin(pin_type_t::in);
                pin->user_pointer = p->user_pointer;
                graph->user_ptr_to_pin[p->user_pointer] = pin;
                m[pin] = p;
            }
            for (auto p : n->out_pins)
            {
                auto pin = node->add_pin(pin_type_t::in);
                pin->user_pointer = p->user_pointer;
                graph->user_ptr_to_pin[p->user_pointer] = pin;
                m[pin] = p;
            }
        }
        for (auto n : nodes)
        {
            for (auto e : n->in_edges)
            {
                pin_t* tail = m[e->tail];
                pin_t* head = m[e->head];
                graph->add_edge(tail, head);
            }
            for (auto e : n->out_edges)
            {
                pin_t* tail = m[e->tail];
                pin_t* head = m[e->head];
                graph->add_edge(tail, head);
            }
        }
        return graph;
    }

    void connected_graph_t::merge_edges()
    {
        map<pair<node_t*, node_t*>, vector<edge_t*>> tail_to_head_edges_map;
        for (auto [fst, edge] : edges)
        {
            pair p = make_pair(edge->tail->owner, edge->head->owner);
            tail_to_head_edges_map[p].push_back(edge);
        }
        for (auto [k, v] : tail_to_head_edges_map)
        {
            if (v.size() > 1)
            {
                edge_t* first_edge = v[0];
                for (size_t i = 1; i < v.size(); i++)
                {
                    first_edge->weight += v[i]->weight;
                    remove_edge(v[i]);
                }
            }
        }
    }

    vector<pin_t*> connected_graph_t::get_pins() const
    {
        vector<pin_t*> pins;
        for (auto n : nodes)
        {
            pins.reserve(pins.size() + n->in_pins.size() + n->out_pins.size());
            pins.insert(pins.end(), n->in_pins.begin(), n->in_pins.end());
            pins.insert(pins.end(), n->out_pins.begin(), n->out_pins.end());
        }
        return pins;
    }

    vector<node_t*> connected_graph_t::get_source_nodes() const
    {
        vector<node_t*> source_nodes;
        for (auto n : nodes)
        {
            if (n->in_edges.empty())
            {
                source_nodes.push_back(n);
            }
        }
        return source_nodes;
    }

    vector<node_t*> connected_graph_t::get_sink_nodes() const
    {
        vector<node_t*> source_nodes;
        for (auto n : nodes)
        {
            if (n->out_edges.empty())
            {
                source_nodes.push_back(n);
            }
        }
        return source_nodes;
    }

    void connected_graph_t::translate(vector2_t offset)
    {
        for (auto n : nodes)
        {
            n->set_position(n->position + offset);
        }
        bound = bound.offset_by(offset);
    }

    void graph_t::set_position(vector2_t position)
    {
        translate(vector2_t{position.x - bound.l, position.y - bound.t});
    }

    void disconnected_graph_t::add_graph(graph_t* graph)
    {
        connected_graphs.push_back(graph);
    }

    void disconnected_graph_t::translate(vector2_t offset)
    {
        for (auto graph : connected_graphs)
        {
            graph->translate(offset);
        }
    }

    std::vector<pin_t*> disconnected_graph_t::get_pins() const
    {
        std::set<pin_t*> set;
        for (auto graph : connected_graphs)
        {
            auto pins = graph->get_pins();
            std::copy(pins.begin(), pins.end(), std::inserter(set, set.end()));
        }
        std::vector<pin_t*> result;
        std::copy(set.begin(), set.end(), std::inserter(result, result.end()));
        return result;
    }

    std::map<pin_t*, vector2_t> disconnected_graph_t::get_pins_offset()
    {
        std::map<pin_t*, vector2_t> result;
        for (auto graph : connected_graphs)
        {
            const auto& sub_bound = graph->bound;
            auto offset = vector2_t{sub_bound.l, sub_bound.t} - vector2_t{bound.l, bound.t};
            auto sub_offsets = graph->get_pins_offset();
            for (auto& [pin, v] : sub_offsets)
            {
                v = v + offset;
            }
            std::copy(sub_offsets.begin(), sub_offsets.end(), std::inserter(result, result.end()));
        }
        return result;
    }

    std::map<node_t*, rect_t> disconnected_graph_t::get_bounds()
    {
        std::map<node_t*, rect_t> result;
        for (auto graph : connected_graphs)
        {
            auto bounds = graph->get_bounds();
            std::copy(bounds.begin(), bounds.end(), std::inserter(result, result.end()));
        }
        return result;
    }

    void disconnected_graph_t::arrange()
    {
        rect_t pre_bound;
        bool bound_valid = false;
        for (auto graph : connected_graphs)
        {
            graph->arrange();

            if (bound_valid)
            {
                vector2_t start_corner = is_vertical_layout ? vector2_t{pre_bound.r, pre_bound.t} : vector2_t{pre_bound.l, pre_bound.b};
                graph->set_position(start_corner);
            }
            auto Bound = graph->bound;
            if (bound_valid)
            {
                bound = bound.expand(Bound);
            }
            else
            {
                bound = Bound;
                bound_valid = true;
            }

            vector2_t offset = is_vertical_layout ? vector2_t{spacing.y, 0} : vector2_t{0, spacing.y};
            pre_bound = bound.offset_by(offset);
        }
    }

    std::set<void*> disconnected_graph_t::get_user_pointers()
    {
        set<void*> result;
        for (auto graph : connected_graphs)
        {
            auto user_pointers = graph->get_user_pointers();
            std::copy(user_pointers.begin(), user_pointers.end(), std::inserter(result, result.end()));
        }
        return result;
    }

    disconnected_graph_t::~disconnected_graph_t()
    {
        for (auto graph : connected_graphs)
        {
            delete graph;
        }
    }

    void connected_graph_t::acyclic() const
    {
        if (nodes.empty())
        {
            return;
        }
        set<node_t*> visited_set;
        vector<edge_t*> non_tree_edges;
        map<node_t*, node_t*> nodes_map;
        map<pin_t*, pin_t*> pins_map;
        map<edge_t*, edge_t*> edges_map;
        map<node_t*, node_t*> nodes_map_inv;
        map<pin_t*, pin_t*> pins_map_inv;
        map<edge_t*, edge_t*> edges_map_inv;
        connected_graph_t* tree = clone(nodes_map, pins_map, edges_map, nodes_map_inv, pins_map_inv, edges_map_inv);
        vector<node_t*> source_nodes = tree->get_source_nodes();
        if (!source_nodes.empty())
        {
            for (auto n : source_nodes)
            {
                visited_set.insert(n);
                dfs(n, visited_set, nullptr, [&non_tree_edges](edge_t* e)
                {
                    non_tree_edges.push_back(e);
                });
            }
        }
        else
        {
            vector<node_t*> sink_nodes = tree->get_sink_nodes();
            if (!sink_nodes.empty())
            {
                for (auto n : sink_nodes)
                {
                    visited_set.insert(n);
                    dfs_inv(n, visited_set, nullptr, [&non_tree_edges](edge_t* e)
                    {
                        non_tree_edges.push_back(e);
                    });
                }
            }
            else
            {
                visited_set.insert(tree->nodes[0]);
                dfs(tree->nodes[0], visited_set, nullptr, [&non_tree_edges](edge_t* e)
                {
                    non_tree_edges.push_back(e);
                });
            }
        }

        vector<pair<node_t*, node_t*>> node_pairs;
        vector<edge_t*> original_non_tree_edges;
        for (auto e : non_tree_edges)
        {
            auto tail_node = e->tail->owner;
            auto head_node = e->head->owner;
            original_non_tree_edges.push_back(edges_map[e]);
            node_pairs.emplace_back(tail_node, head_node);
            tree->remove_edge(e);
        }
        for (size_t i = 0; i < node_pairs.size(); i++)
        {
            auto p = node_pairs[i];
            if (p.first->is_descendant_of(p.second))
            {
                invert_edge(original_non_tree_edges[i]);
                printf("invert edge: %s->%s\n", original_non_tree_edges[i]->head->owner->name.c_str(), original_non_tree_edges[i]->tail->owner->name.c_str());
            }
        }
        delete tree;
    }

    void connected_graph_t::rank() const
    {
        tree_t tree = feasible_tree();
        tree.calculate_cut_values();
        while (edge_t* e = tree.leave_edge())
        {
            edge_t* f = tree.enter_edge(e);
            tree.exchange(e, f);
        }
        normalize();
    }

    void connected_graph_t::add_dummy_nodes(tree_t* feasible_tree)
    {
        vector<edge_t*> edges_vec;
        transform(edges.begin(), edges.end(), back_inserter(edges_vec), [](auto& p) { return p.second; });
        for (auto edge : edges_vec)
        {
            int edge_len = edge->length();
            if (edge_len > 1)
            {
                bool is_tree_edge = feasible_tree && feasible_tree->tree_edges.find(edge) != feasible_tree->tree_edges.end();
                pin_t* tail = edge->tail;
                for (int i = 0; i < edge_len - 1; i++)
                {
                    node_t* dummy = add_node("dummy");
                    dummy->is_dummy_node = true;
                    if (is_tree_edge) feasible_tree->nodes.insert(dummy);
                    dummy->rank = edge->tail->owner->rank + i + 1;
                    pin_t* dummy_in = dummy->add_pin(pin_type_t::in);
                    pin_t* dummy_out = dummy->add_pin(pin_type_t::out);
                    edge_t* dummy_edge = add_edge(tail, dummy_in);
                    if (is_tree_edge) feasible_tree->tree_edges.insert(dummy_edge);
                    tail = dummy_out;
                }
                edge_t* dummy_edge = add_edge(tail, edge->head);
                if (is_tree_edge)
                {
                    feasible_tree->tree_edges.insert(dummy_edge);
                    feasible_tree->tree_edges.erase(edge);
                }
                remove_edge(edge);
            }
        }
    }

    void connected_graph_t::assign_layers()
    {
        map<int, vector<node_t*>> rank_layer_map;
        for (auto n : nodes)
        {
            rank_layer_map[n->rank].push_back(n);
        }
        layers.resize(rank_layer_map.size());
        for (auto [rank, layer] : rank_layer_map)
        {
            layers[rank] = std::move(layer);
        }
    }

    void connected_graph_t::ordering()
    {
        auto order = layers;
        auto best = layers;
        size_t best_crossing = crossing(best, true);
        for (size_t i = 0; i < max_iterations; i++)
        {
            sort_layers(order, i % 2 == 0);
            const size_t new_crossing = crossing(order, true);
            if (new_crossing < best_crossing)
            {
                best = order;
                best_crossing = new_crossing;
            }
        }
        layers = best;
    }

    void connected_graph_t::arrange()
    {
        for (auto [node, graph] : sub_graphs)
        {
            graph->arrange();
            node->update_pins_offset();
            auto sub_bound = graph->bound;
            node->position = vector2_t{sub_bound.l, sub_bound.t} - vector2_t{graph->border.l, graph->border.t};
            node->size = sub_bound.size() + graph->border.size() * 2;
        }
        if (!nodes.empty())
        {
            acyclic();
            rank();
            add_dummy_nodes(nullptr);
            assign_layers();
            ordering();
            assign_coordinate();
        }
    }

    void connected_graph_t::assign_coordinate()
    {
        auto layers_bound = get_layers_bound();
        fas_positioning_strategy_t positioning_strategy{layers, !is_vertical_layout, layers_bound};
        positioning_strategy.assign_coordinate();
    }

    std::map<pin_t*, vector2_t> connected_graph_t::get_pins_offset()
    {
        map<pin_t*, vector2_t> result;
        for (auto n : nodes)
        {
            for (auto pin : n->out_pins)
            {
                vector2_t offset = n->position + pin->offset - vector2_t{bound.t, bound.l};
                result[pin] = offset;
            }
            for (auto pin : n->in_pins)
            {
                vector2_t offset = n->position + pin->offset - vector2_t{bound.t, bound.l};
                result[pin] = offset;
            }
        }
        return result;
    }

    std::map<node_t*, rect_t> connected_graph_t::get_bounds()
    {
        std::map<node_t*, rect_t> result;
        for (auto n : nodes)
        {
            if (n->is_dummy_node) continue;
            float right = n->position.x + n->size.x;
            float bottom = n->position.y + n->size.y;
            result[n] = rect_t{n->position.x, n->position.y, right, bottom};
            if (n->graph)
            {
                auto sub_bounds = n->graph->get_bounds();
                std::copy(sub_bounds.begin(), sub_bounds.end(), std::inserter(result, result.end()));
            }
        }
        return result;
    }

    std::vector<rect_t> connected_graph_t::get_layers_bound() const
    {
        vector<rect_t> layers_bound;
        rect_t total_bound{0, 0, -spacing.x, -spacing.y};
        for (const auto& layer : layers)
        {
            vector2_t position = vector2_t{total_bound.r, total_bound.b} + spacing;
            rect_t layer_bound{position.x, position.y, position.x, position.y};
            for (auto n : layer)
            {
                layer_bound = layer_bound.expand(position, n->size);
            }
            layers_bound.push_back(layer_bound);
            total_bound = total_bound.expand(layer_bound);
        }
        return layers_bound;
    }

    void connected_graph_t::sort_layers(std::vector<std::vector<node_t*>>& layer_vec, bool is_down) const
    {
        int max_rank = static_cast<int>(layer_vec.size());
        if (max_rank < 2)
        {
            return;
        }
        const int start = is_down ? 1 : max_rank - 2;
        const int end = is_down ? max_rank : -1;
        const int step = is_down ? 1 : -1;
        int i = start;
        auto& first_layer = layer_vec[i - step];
        calculate_pins_index_in_layer(first_layer);
        while (i != end)
        {
            auto& fixed_layer = layer_vec[i - step];
            auto& free_layer = layer_vec[i];
            for (auto n : free_layer)
            {
                n->layer_order = n->get_barycenter_in_layer(fixed_layer, is_down);
            }
            stable_sort(free_layer.begin(), free_layer.end(), [](node_t* a, node_t* b)
            {
                if (a->layer_order == -1.0f || b->layer_order == -1.0f) return false;
                return a->layer_order < b->layer_order;
            });
            calculate_pins_index_in_layer(free_layer);
            i += step;
        }
    }

    void connected_graph_t::calculate_pins_index_in_layer(const std::vector<node_t*>& layer)
    {
        if (layer.empty())
        {
            return;
        }
        int in_pin_start_index = 0;
        int out_pin_start_index = 0;
        int end = static_cast<int>(layer.size());
        for (int i = 0; i != end; i++)
        {
            for (int j = 0; j < layer[i]->in_pins.size(); j++)
            {
                auto pin = layer[i]->in_pins[j];
                pin->index_in_layer = in_pin_start_index + j;
            }
            for (int j = 0; j < layer[i]->out_pins.size(); j++)
            {
                auto pin = layer[i]->out_pins[j];
                pin->index_in_layer = out_pin_start_index + j;
            }
            out_pin_start_index += layer[i]->out_pins.size();
            in_pin_start_index += layer[i]->in_pins.size();
        }
    }

    std::vector<edge_t*> connected_graph_t::get_edges_between_two_layers(const vector<node_t*>& lower, const vector<node_t*>& upper, const node_t* excluded_node)
    {
        vector<edge_t*> result;
        for (auto n : lower)
        {
            if (excluded_node == n)
            {
                continue;
            }
            auto layer_edges = n->get_edges_linked_to_layer(upper, true);
            result.insert(result.end(), layer_edges.begin(), layer_edges.end());
        }
        return result;
    }

    size_t connected_graph_t::crossing(const vector<vector<node_t*>>& order, bool calculate_pins_index)
    {
        size_t crossing_value = 0;
        if (calculate_pins_index)
        {
            for (auto& layer : order)
            {
                calculate_pins_index_in_layer(layer);
            }
        }
        for (int i = 1; i < order.size(); i++)
        {
            auto& upper_layer = order[i - 1];
            auto& lower_layer = order[i];
            auto cross_edges = get_edges_between_two_layers(lower_layer, upper_layer);
            while (!cross_edges.empty())
            {
                auto edge1 = cross_edges.back();
                cross_edges.pop_back();
                for (auto edge2 : cross_edges)
                {
                    if (edge1->is_crossing(edge2))
                    {
                        crossing_value++;
                    }
                }
            }
        }
        return crossing_value;
    }

    tree_t connected_graph_t::feasible_tree() const
    {
        init_rank();
        for (;;)
        {
            tree_t tree = tight_tree();
            if (tree.nodes.size() == nodes.size())
            {
                return tree;
            }
            node_t* incident_node;
            edge_t* e = tree.find_min_incident_edge(&incident_node);
            auto delta = e->slack();
            if (e->head->owner == incident_node)
            {
                delta = -delta;
            }
            for (auto n : tree.nodes)
            {
                n->rank = n->rank + delta;
            }
        }
    }

    string connected_graph_t::generate_test_code()
    {
        stringstream ss;
        ss << "connected_graph_t g;" << std::endl;
        for (auto n : nodes)
        {
            string node_name = n->name;
            replace(node_name.begin(), node_name.end(), ' ', '_');
            ss << "auto node_" << node_name << " = g.add_node(\"" << node_name << "\");" << std::endl;
            int pin_index = 0;
            for (auto p : n->in_pins)
            {
                string in_or_out = p->type == pin_type_t::in ? "in" : "out";
                ss << "auto pin_" << node_name << "_" << in_or_out << pin_index << " = node_" << node_name << "->add_pin(pin_type_t::" << in_or_out << ");" << endl;
                pin_index ++;
            }
            for (auto p : n->out_pins)
            {
                string in_or_out = p->type == pin_type_t::in ? "in" : "out";
                ss << "auto pin_" << node_name << "_" << in_or_out << pin_index << " = node_" << node_name << "->add_pin(pin_type_t::" << in_or_out << ");" << endl;
                pin_index ++;
            }
            ss << endl;
        }
        auto find_in_pin_index = [](node_t* n, pin_t* p)
        {
            for (int i = 0; i < n->in_pins.size(); i++)
            {
                if (n->in_pins[i] == p) return i;
            }
            return -1;
        };
        auto find_out_pin_index = [](node_t* n, pin_t* p)
        {
            for (int i = 0; i < n->out_pins.size(); i++)
            {
                if (n->out_pins[i] == p) return i;
            }
            return -1;
        };
        for (auto [k, edge] : edges)
        {
            auto tail_name = edge->tail->owner->name;
            auto head_name = edge->head->owner->name;

            replace(tail_name.begin(), tail_name.end(), ' ', '_');
            replace(head_name.begin(), head_name.end(), ' ', '_');
            ss << "g.add_edge(" << "pin_" << tail_name << "_out" << find_out_pin_index(edge->tail->owner, edge->tail)
                << ", " << "pin_" << head_name << "_in" << find_in_pin_index(edge->head->owner, edge->head) << ");" << endl;
        }
        ofstream ofile;
        ofile.open("test.cpp");
        ofile << ss.str();
        ofile.close();
        return ss.str();
    }

    void connected_graph_t::init_rank() const
    {
        set<edge_t*> scanned_set;
        int ranking = 0;
        set<node_t*> visited_set;
        while (visited_set.size() != nodes.size())
        {
            auto queue = get_nodes_without_unscanned_in_edges(visited_set, scanned_set);
            while (!queue.empty())
            {
                auto n = queue.back();
                n->rank = ranking;
                visited_set.insert(n);
                for (auto e : n->out_edges)
                {
                    scanned_set.insert(e);
                }
                queue.pop_back();
            }
            ranking++;
        }
    }

    void connected_graph_t::normalize() const
    {
        int min_rank = std::numeric_limits<int>::max();
        for (auto n : nodes)
        {
            if (n->rank < min_rank)
            {
                min_rank = n->rank;
            }
        }
        for (auto n : nodes)
        {
            n->rank -= min_rank;
        }
    }

    tree_t connected_graph_t::tight_tree() const
    {
        tree_t tree;
        vector<node_t*> stack;
        int min_slack = std::numeric_limits<int>::max();
        if (min_ranking_node)
        {
            stack.push_back(min_ranking_node);
        }
        else
        {
            if (max_ranking_node)
            {
                stack.push_back(max_ranking_node);
            }
            else
            {
                stack.push_back(nodes[0]);
            }
        }
        while (!stack.empty())
        {
            auto n = stack.back();
            tree.nodes.insert(n);
            stack.pop_back();
            auto connected_nodes = n->get_direct_connected_nodes([&min_slack, n ,&tree](edge_t* e)
            {
                if (e->slack() != 0)
                {
                    if (e->slack() < min_slack)
                    {
                        min_slack = e->slack();
                    }
                    return false;
                }
                tree.tree_edges.insert(e);
                return true;
            });
            for (auto connected_node : connected_nodes)
            {
                if (tree.nodes.find(connected_node) == tree.nodes.end())
                {
                    stack.push_back(connected_node);
                }
            }
        }
        set<edge_t*> all_edges;
        for (auto [fst, edge] : edges)
        {
            all_edges.insert(edge);
        }
        tree.update_non_tree_edges(all_edges);
        return tree;
    }

    edge_t* tree_t::find_min_incident_edge(node_t** incident_node)
    {
        edge_t* min_slack_edge = nullptr;
        int slack = std::numeric_limits<int>::max();
        *incident_node = nullptr;
        for (auto e : non_tree_edges)
        {
            bool head_is_tree_node = nodes.find(e->head->owner) != nodes.end();
            bool tail_is_tree_node = nodes.find(e->tail->owner) != nodes.end();
            if ((head_is_tree_node && !tail_is_tree_node) || (!head_is_tree_node && tail_is_tree_node))
            {
                if (e->slack() < slack)
                {
                    min_slack_edge = e;
                    slack = e->slack();
                    *incident_node = head_is_tree_node ? e->head->owner : e->tail->owner;
                }
            }
        }
        return min_slack_edge;
    }

    void tree_t::tighten() const
    {
        for (;;)
        {
            tree_t tree = tight_sub_tree();
            if (tree.nodes.size() == nodes.size())
            {
                return;
            }
            node_t* incident_node;
            edge_t* e = tree.find_min_incident_edge(&incident_node);
            auto delta = e->slack();
            if (e->head->owner == incident_node)
            {
                delta = -delta;
            }
            for (auto n : tree.nodes)
            {
                n->rank = n->rank + delta;
            }
        }
    }

    tree_t tree_t::tight_sub_tree() const
    {
        tree_t tree;
        int min_slack = std::numeric_limits<int>::max();
        vector stack{*nodes.begin()};
        while (!stack.empty())
        {
            auto n = stack.back();
            tree.nodes.insert(n);
            stack.pop_back();
            auto connected_nodes = n->get_direct_connected_nodes([&min_slack, &tree, this](edge_t* e)
            {
                if (tree_edges.find(e) == tree_edges.end())
                {
                    return false;
                }
                if (e->slack() != 0)
                {
                    if (e->slack() < min_slack)
                    {
                        min_slack = e->slack();
                    }
                    return false;
                }
                tree.tree_edges.insert(e);
                return true;
            });
            for (auto connected_node : connected_nodes)
            {
                if (tree.nodes.find(connected_node) == tree.nodes.end())
                {
                    stack.push_back(connected_node);
                }
            }
        }
        tree.update_non_tree_edges(tree_edges);
        return tree;
    }

    edge_t* tree_t::leave_edge() const
    {
        for (auto edge : tree_edges)
        {
            if (edge->cut_value < 0)
            {
                return edge;
            }
        }
        return nullptr;
    }

    edge_t* tree_t::enter_edge(edge_t* edge)
    {
        split_to_head_tail(edge);
        int slack = std::numeric_limits<int>::max();
        edge_t* min_slack_edge = nullptr;
        for (auto e : non_tree_edges)
        {
            if (e->tail->owner->belongs_to_head && e->head->owner->belongs_to_tail)
            {
                if (e->slack() < slack)
                {
                    slack = e->slack();
                    min_slack_edge = e;
                }
            }
        }
        assert(min_slack_edge!=nullptr);
        return min_slack_edge;
    }

    void tree_t::exchange(edge_t* e, edge_t* f)
    {
        tree_edges.insert(f);
        tree_edges.erase(e);
        non_tree_edges.erase(f);
        non_tree_edges.insert(e);
        tighten();
        calculate_cut_values();
    }

    void tree_t::calculate_cut_values()
    {
        set<node_t*> visited_nodes;
        for (auto edge : tree_edges)
        {
            split_to_head_tail(edge);
            int head_to_tail_weight = 0;
            int tail_to_head_weight = 0;
            for (auto edge2 : tree_edges)
            {
                if (edge2 == edge)
                {
                    continue;
                }
                add_to_weights(edge2, head_to_tail_weight, tail_to_head_weight);
            }
            for (auto edge2 : non_tree_edges)
            {
                if (edge2 == edge)
                {
                    continue;
                }
                add_to_weights(edge2, head_to_tail_weight, tail_to_head_weight);
            }
            edge->cut_value = edge->weight + tail_to_head_weight - head_to_tail_weight;
        }
    }

    void tree_t::reset_head_or_tail() const
    {
        for (auto n : nodes)
        {
            n->belongs_to_head = false;
            n->belongs_to_tail = false;
        }
    }

    void tree_t::split_to_head_tail(edge_t* edge)
    {
        reset_head_or_tail();
        mark_head_or_tail(edge->tail->owner, edge, false);
        mark_head_or_tail(edge->head->owner, edge, true);
    }

    void tree_t::mark_head_or_tail(node_t* n, edge_t* cut_edge, bool is_head)
    {
        set<node_t*> visited_nodes;
        vector<node_t*> stack;
        stack.push_back(n);
        while (!stack.empty())
        {
            auto node = stack.back();
            stack.pop_back();
            visited_nodes.insert(node);
            if (is_head)
            {
                node->belongs_to_head = true;
            }
            else
            {
                node->belongs_to_tail = true;
            }
            auto direct_connected_nodes = node->get_direct_connected_nodes([this, cut_edge](edge_t* e)
            {
                if (tree_edges.find(e) == tree_edges.end() || e == cut_edge)
                {
                    return false;
                }
                return true;
            });
            for (auto direct_connected : direct_connected_nodes)
            {
                if (visited_nodes.find(direct_connected) == visited_nodes.end())
                {
                    stack.push_back(direct_connected);
                }
            }
        }
    }

    void tree_t::add_to_weights(const edge_t* edge, int& head_to_tail_weight, int& tail_to_head_weight)
    {
        auto tail = edge->tail->owner;
        auto head = edge->head->owner;
        if (tail->belongs_to_tail && head->belongs_to_head)
        {
            tail_to_head_weight += edge->weight;
        }
        if (tail->belongs_to_head && head->belongs_to_tail)
        {
            head_to_tail_weight += edge->weight;
        }
    }

    void graph_t::translate(vector2_t offset)
    {
    }

    graph_t::~graph_t()
    {
        for (auto [k, edge] : edges)
        {
            delete edge;
        }
        for (auto n : nodes)
        {
            delete n;
        }
    }

    void tree_t::update_non_tree_edges(const set<edge_t*>& all_edges)
    {
        non_tree_edges.clear();
        for (auto edge : all_edges)
        {
            if (tree_edges.find(edge) == tree_edges.end())
            {
                non_tree_edges.insert(edge);
            }
        }
    }

    vector<node_t*> connected_graph_t::get_nodes_without_unscanned_in_edges(const set<node_t*>& visited, const set<edge_t*>& scanned_set) const
    {
        vector<node_t*> result;
        for (auto n : nodes)
        {
            if (visited.find(n) != visited.end())
            {
                continue;
            }
            bool is_all_scanned = true;
            for (auto e : n->in_edges)
            {
                if (scanned_set.find(e) == scanned_set.end())
                {
                    is_all_scanned = false;
                    break;
                }
            }
            if (is_all_scanned)
            {
                result.push_back(n);
            }
        }
        return result;
    }

    std::set<void*> connected_graph_t::get_user_pointers()
    {
        std::set<void*> result;
        for (auto node : nodes)
        {
            if (node->graph)
            {
                auto user_pointers = node->graph->get_user_pointers();
                std::copy(user_pointers.begin(), user_pointers.end(), std::inserter(result, result.end()));
            }
            if (node->user_ptr != nullptr)
            {
                result.insert(node->user_ptr);
            }
        }
        return result;
    }

    void connected_graph_t::test()
    {
        connected_graph_t g;
        auto node_K2Node_CallFunction_18 = g.add_node("K2Node_CallFunction_18");
        auto pin_K2Node_CallFunction_18_in0 = node_K2Node_CallFunction_18->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_18_in1 = node_K2Node_CallFunction_18->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_18_in2 = node_K2Node_CallFunction_18->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_18_out3 = node_K2Node_CallFunction_18->add_pin(pin_type_t::out);

        auto node_K2Node_CallFunction_14 = g.add_node("K2Node_CallFunction_14");
        auto pin_K2Node_CallFunction_14_in0 = node_K2Node_CallFunction_14->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_14_in1 = node_K2Node_CallFunction_14->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_14_in2 = node_K2Node_CallFunction_14->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_14_out3 = node_K2Node_CallFunction_14->add_pin(pin_type_t::out);

        auto node_K2Node_AddComponent_2 = g.add_node("K2Node_AddComponent_2");
        auto pin_K2Node_AddComponent_2_in0 = node_K2Node_AddComponent_2->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_2_in1 = node_K2Node_AddComponent_2->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_2_in2 = node_K2Node_AddComponent_2->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_2_in3 = node_K2Node_AddComponent_2->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_2_in4 = node_K2Node_AddComponent_2->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_2_in5 = node_K2Node_AddComponent_2->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_2_in6 = node_K2Node_AddComponent_2->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_2_out7 = node_K2Node_AddComponent_2->add_pin(pin_type_t::out);
        auto pin_K2Node_AddComponent_2_out8 = node_K2Node_AddComponent_2->add_pin(pin_type_t::out);

        auto node_K2Node_SwitchEnum_0 = g.add_node("K2Node_SwitchEnum_0");
        auto pin_K2Node_SwitchEnum_0_in0 = node_K2Node_SwitchEnum_0->add_pin(pin_type_t::in);
        auto pin_K2Node_SwitchEnum_0_in1 = node_K2Node_SwitchEnum_0->add_pin(pin_type_t::in);
        auto pin_K2Node_SwitchEnum_0_in2 = node_K2Node_SwitchEnum_0->add_pin(pin_type_t::in);
        auto pin_K2Node_SwitchEnum_0_out3 = node_K2Node_SwitchEnum_0->add_pin(pin_type_t::out);
        auto pin_K2Node_SwitchEnum_0_out4 = node_K2Node_SwitchEnum_0->add_pin(pin_type_t::out);
        auto pin_K2Node_SwitchEnum_0_out5 = node_K2Node_SwitchEnum_0->add_pin(pin_type_t::out);

        auto node_K2Node_CallFunction_16 = g.add_node("K2Node_CallFunction_16");
        auto pin_K2Node_CallFunction_16_in0 = node_K2Node_CallFunction_16->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_16_in1 = node_K2Node_CallFunction_16->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_16_in2 = node_K2Node_CallFunction_16->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_16_in3 = node_K2Node_CallFunction_16->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_16_out4 = node_K2Node_CallFunction_16->add_pin(pin_type_t::out);
        auto pin_K2Node_CallFunction_16_out5 = node_K2Node_CallFunction_16->add_pin(pin_type_t::out);

        auto node_K2Node_CallFunction_13 = g.add_node("K2Node_CallFunction_13");
        auto pin_K2Node_CallFunction_13_in0 = node_K2Node_CallFunction_13->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_13_in1 = node_K2Node_CallFunction_13->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_13_in2 = node_K2Node_CallFunction_13->add_pin(pin_type_t::in);
        auto pin_K2Node_CallFunction_13_out3 = node_K2Node_CallFunction_13->add_pin(pin_type_t::out);

        auto node_K2Node_AddComponent_4 = g.add_node("K2Node_AddComponent_4");
        auto pin_K2Node_AddComponent_4_in0 = node_K2Node_AddComponent_4->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_4_in1 = node_K2Node_AddComponent_4->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_4_in2 = node_K2Node_AddComponent_4->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_4_in3 = node_K2Node_AddComponent_4->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_4_in4 = node_K2Node_AddComponent_4->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_4_in5 = node_K2Node_AddComponent_4->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_4_in6 = node_K2Node_AddComponent_4->add_pin(pin_type_t::in);
        auto pin_K2Node_AddComponent_4_out7 = node_K2Node_AddComponent_4->add_pin(pin_type_t::out);
        auto pin_K2Node_AddComponent_4_out8 = node_K2Node_AddComponent_4->add_pin(pin_type_t::out);

        g.add_edge(pin_K2Node_SwitchEnum_0_out4, pin_K2Node_AddComponent_2_in0);
        g.add_edge(pin_K2Node_AddComponent_2_out8, pin_K2Node_CallFunction_18_in1);
        g.add_edge(pin_K2Node_AddComponent_2_out7, pin_K2Node_CallFunction_18_in2);
        g.add_edge(pin_K2Node_AddComponent_2_out8, pin_K2Node_CallFunction_14_in1);
        g.add_edge(pin_K2Node_AddComponent_4_out8, pin_K2Node_CallFunction_16_in1);
        g.add_edge(pin_K2Node_AddComponent_4_out8, pin_K2Node_CallFunction_13_in1);
        g.add_edge(pin_K2Node_CallFunction_16_out5, pin_K2Node_CallFunction_18_in2);
        g.add_edge(pin_K2Node_CallFunction_13_out3, pin_K2Node_CallFunction_16_in0);
        g.add_edge(pin_K2Node_CallFunction_14_out3, pin_K2Node_CallFunction_18_in0);

        g.set_node_in_rank_slot(node_K2Node_AddComponent_4, rank_slot_t::min);
        g.acyclic();
        g.rank();
        g.add_dummy_nodes(nullptr);
        g.assign_layers();
        g.ordering();
    }
}