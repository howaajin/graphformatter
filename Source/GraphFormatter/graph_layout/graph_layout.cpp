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

    static void dfs(const node_t* node, set<node_t*>& visited_set, function<void(node_t*)> on_visit, function<void(edge_t*)> on_non_tree_edge_found)
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

    static void dfs_inv(const node_t* node, set<node_t*>& visited_set, function<void(node_t*)> on_visit, function<void(edge_t*)> on_non_tree_edge_found)
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

    bool edge_t::is_crossing(edge_t* other)
    {
        return (tail->index_in_layer < other->tail->index_in_layer && head->index_in_layer > other->head->index_in_layer) ||
            (tail->index_in_layer > other->tail->index_in_layer && head->index_in_layer < other->head->index_in_layer);
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

    std::vector<edge_t*> node_t::get_edges_linked_to_layer(std::vector<node_t*> layer, bool is_in)
    {
        vector<edge_t*> result;
        auto edges = is_in ? in_edges : out_edges;
        for (auto e : edges)
        {
            for (auto layer_node : layer)
            {
                auto n = is_in ? e->head->owner : e->tail->owner;
                if (n == layer_node)
                {
                    result.push_back(e);
                }
            }
        }
        return result;
    }

    float node_t::get_barycenter_in_layer(std::vector<node_t*> layer, bool is_in)
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

    set<node_t*> node_t::get_direct_connected_nodes(function<bool(edge_t*)> filter) const
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

    graph_t* graph_t::clone() const
    {
        map<node_t*, node_t*> nodes_map;
        map<pin_t*, pin_t*> pins_map;
        map<edge_t*, edge_t*> edges_map;
        map<node_t*, node_t*> nodes_map_inv;
        map<pin_t*, pin_t*> pins_map_inv;
        map<edge_t*, edge_t*> edges_map_inv;
        return clone(nodes_map, pins_map, edges_map, nodes_map_inv, pins_map_inv, edges_map_inv);
    }

    graph_t* graph_t::clone(std::map<node_t*, node_t*>& nodes_map, std::map<pin_t*, pin_t*>& pins_map, std::map<edge_t*, edge_t*>& edges_map,
                            std::map<node_t*, node_t*>& nodes_map_inv, std::map<pin_t*, pin_t*>& pins_map_inv, std::map<edge_t*, edge_t*>& edges_map_inv) const
    {
        auto cloned = new graph_t;
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

    void graph_t::set_node_in_rank_slot(node_t* node, rank_slot_t rank_slot)
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
                    //dummy_edge->min_length = 0;
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
                    //dummy_edge->min_length = 0;
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

    void graph_t::merge_edges()
    {
        map<pair<node_t*, node_t*>, vector<edge_t*>> tail_to_head_edges_map;
        for (auto [fst, edge] : edges)
        {
            pair p = make_pair(edge->tail->owner, edge->head->owner);
            auto it = tail_to_head_edges_map.find(p);
            if (it != tail_to_head_edges_map.end())
            {
                it->second.push_back(edge);
            }
            else
            {
                tail_to_head_edges_map.insert(make_pair(p, vector{edge}));
            }
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

    vector<pin_t*> graph_t::get_pins() const
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

    vector<node_t*> graph_t::get_source_nodes() const
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

    vector<node_t*> graph_t::get_sink_nodes() const
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

    void graph_t::translate(vector2_t offset)
    {
        for (auto n : nodes)
        {
            n->set_position(n->position + offset);
        }
        bound.offset_by(offset);
    }

    void graph_t::set_position(vector2_t position)
    {
        translate(vector2_t{position.x - bound.l, position.y - bound.t});
    }

    void graph_t::acyclic() const
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
        graph_t* tree = clone(nodes_map, pins_map, edges_map, nodes_map_inv, pins_map_inv, edges_map_inv);
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

    void graph_t::rank()
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

    void graph_t::add_dummy_nodes(tree_t& feasible_tree)
    {
        vector<edge_t*> edges_vec;
        transform(edges.begin(), edges.end(), back_inserter(edges_vec), [](auto& p) { return p.second; });
        for (auto edge : edges_vec)
        {
            int edge_len = edge->length();
            if (edge_len > 1)
            {
                bool is_tree_edge = feasible_tree.tree_edges.find(edge) != feasible_tree.tree_edges.end();
                pin_t* tail = edge->tail;
                for (int i = 0; i < edge_len - 1; i++)
                {
                    node_t* dummy = add_node("dummy");
                    if (is_tree_edge) feasible_tree.nodes.insert(dummy);
                    dummy->rank = edge->tail->owner->rank + i + 1;
                    pin_t* dummy_in = dummy->add_pin(pin_type_t::in);
                    pin_t* dummy_out = dummy->add_pin(pin_type_t::out);
                    edge_t* dummy_edge = add_edge(tail, dummy_in);
                    if (is_tree_edge) feasible_tree.tree_edges.insert(dummy_edge);
                    tail = dummy_out;
                }
                edge_t* dummy_edge = add_edge(tail, edge->head);
                if (is_tree_edge)
                {
                    feasible_tree.tree_edges.insert(dummy_edge);
                    feasible_tree.tree_edges.erase(edge);
                }
                remove_edge(edge);
            }
        }
    }

    void graph_t::assign_layers()
    {
        map<int, vector<node_t*>> rank_layer_map;
        queue<node_t*> queue;
        set<node_t*> visited;
        for (auto n : nodes)
        {
            if (n->rank == 0)
            {
                queue.push(n);
                visited.insert(n);
            }
        }
        while (!queue.empty())
        {
            node_t* node = queue.front();
            queue.pop();
            auto it = rank_layer_map.find(node->rank);
            if (it != rank_layer_map.end())
            {
                it->second.push_back(node);
            }
            else
            {
                rank_layer_map.insert(make_pair(node->rank, vector{node}));
            }
            auto out_nodes = node->get_out_nodes();
            for (auto n : out_nodes)
            {
                if (visited.find(n) == visited.end())
                {
                    visited.insert(n);
                    queue.push(n);
                }
            }
        }
        layers.resize(rank_layer_map.size());
        for (auto [rank, layer] : rank_layer_map)
        {
            layers[rank] = std::move(layer);
        }
    }

    std::vector<std::vector<node_t*>> graph_t::ordering()
    {
        auto order = layers;
        auto best = layers;
        size_t best_crossing = crossing(best, true);
        for (size_t i = 0; i < max_iterations; i++)
        {
            sort_layers(order, i % 2 == 0);
            const size_t new_crossing = crossing(order, false);
            if (new_crossing < best_crossing)
            {
                best = order;
                best_crossing = new_crossing;
            }
        }
        return best;
    }

    void graph_t::sort_layers(std::vector<std::vector<node_t*>> layer_vec, bool is_down)
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

    void graph_t::calculate_pins_index_in_layer(std::vector<node_t*>& layer) const
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
            for (int j = 0; j < layer[i]->in_pins.size(); i++)
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

    std::vector<edge_t*> graph_t::get_edges_between_two_layers(std::vector<node_t*> lower, std::vector<node_t*> upper) const
    {
        vector<edge_t*> result;
        for (auto n : lower)
        {
            auto layer_edges = n->get_edges_linked_to_layer(upper, false);
            result.insert(result.end(), layer_edges.begin(), layer_edges.end());
        }
        return result;
    }

    size_t graph_t::crossing(std::vector<std::vector<node_t*>>& order, bool calculate_pins_index) const
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

    tree_t graph_t::feasible_tree()
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

    std::string graph_t::generate_test_code()
    {
        stringstream ss;
        ss << "graph_t g;" << std::endl;
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

    void graph_t::init_rank()
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

    void graph_t::normalize()
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

    tree_t graph_t::tight_tree() const
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

    vector<node_t*> graph_t::get_nodes_without_unscanned_in_edges(const set<node_t*>& visited, const set<edge_t*>& scanned_set) const
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

    void graph_t::test()
    {
        graph_t g;
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

        map<node_t*, node_t*> nodes_map;
        map<pin_t*, pin_t*> pins_map;
        map<edge_t*, edge_t*> edges_map;
        map<node_t*, node_t*> nodes_map_inv;
        map<pin_t*, pin_t*> pins_map_inv;
        map<edge_t*, edge_t*> edges_map_inv;
        graph_t* cloned = g.clone(nodes_map, pins_map, edges_map, nodes_map_inv, pins_map_inv, edges_map_inv);
        cloned->set_node_in_rank_slot(nodes_map_inv[node_K2Node_AddComponent_4], rank_slot_t::min);
        cloned->merge_edges();
        cloned->acyclic();
        cloned->rank();
        for (auto n : cloned->nodes)
        {
            nodes_map[n]->rank = n->rank;
        }
        delete cloned;
    }
}
