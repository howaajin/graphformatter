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
        pins.push_back(pin);
        return pin;
    }

    vector<node_t*> node_t::get_direct_connected_nodes(function<bool(edge_t*)> filter) const
    {
        vector<node_t*> result;
        for (auto e : in_edges)
        {
            if (filter(e))
            {
                result.push_back(e->tail->owner);
            }
        }
        for (auto e : out_edges)
        {
            if (filter(e))
            {
                result.push_back(e->head->owner);
            }
        }
        return result;
    }

    node_t::~node_t()
    {
        for (auto pin : pins)
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
        for (auto pin : pins)
        {
            auto new_pin = new pin_t();
            new_pin->type = pin->type;
            new_pin->offset = pin->offset;
            new_pin->owner = new_node;
            new_node->pins.push_back(new_pin);
        }
        return new_node;
    }

    graph_t* graph_t::clone() const
    {
        unordered_map<node_t*, node_t*> nodes_map;
        unordered_map<pin_t*, pin_t*> pins_map;
        unordered_map<edge_t*, edge_t*> edges_map;
        return clone(nodes_map, pins_map, edges_map);
    }

    graph_t* graph_t::clone(unordered_map<node_t*, node_t*>& nodes_map, unordered_map<pin_t*, pin_t*>& pins_map, unordered_map<edge_t*, edge_t*>& edges_map) const
    {
        auto cloned = new graph_t;
        cloned->bound = bound;
        unordered_map<pin_t*, pin_t*> pins_map_inv;
        for (auto n : nodes)
        {
            auto cloned_node = n->clone();
            nodes_map.insert(make_pair(cloned_node, n));
            for (size_t i = 0; i < n->pins.size(); i++)
            {
                pins_map_inv.insert(make_pair(n->pins[i], cloned_node->pins[i]));
                pins_map.insert(make_pair(cloned_node->pins[i], n->pins[i]));
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
            edges_map.insert(make_pair(edge, e.second));
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
        if (edge->tail->type == pin_type_t::in)
        {
            auto& out_edges = edge->head->owner->out_edges;
            auto& in_edges = edge->tail->owner->in_edges;
            in_edges.erase(find(in_edges.begin(), in_edges.end(), edge));
            edge->tail->type = pin_type_t::out;
            edge->tail->owner->out_edges.push_back(edge);
            out_edges.erase(find(out_edges.begin(), out_edges.end(), edge));
            edge->head->type = pin_type_t::in;
            edge->head->owner->in_edges.push_back(edge);
        }
        else
        {
            auto& out_edges = edge->tail->owner->out_edges;
            auto& in_edges = edge->head->owner->in_edges;
            out_edges.erase(find(out_edges.begin(), out_edges.end(), edge));
            edge->tail->type = pin_type_t::in;
            edge->tail->owner->in_edges.push_back(edge);
            in_edges.erase(find(in_edges.begin(), in_edges.end(), edge));
            edge->head->type = pin_type_t::out;
            edge->head->owner->out_edges.push_back(edge);
        }
        edge->is_inverted = true;
    }

    vector<pin_t*> graph_t::get_pins() const
    {
        vector<pin_t*> pins;
        for (auto n : nodes)
        {
            pins.reserve(pins.size() + n->pins.size());
            pins.insert(pins.end(), n->pins.begin(), n->pins.end());
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

    void graph_t::acyclic()
    {
        if (nodes.empty())
        {
            return;
        }
        set<node_t*> visited_set;
        vector<edge_t*> non_tree_edges;
        unordered_map<node_t*, node_t*> nodes_map;
        unordered_map<pin_t*, pin_t*> pins_map;
        unordered_map<edge_t*, edge_t*> edges_map;
        graph_t* tree = clone(nodes_map, pins_map, edges_map);
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
                printf("invert edge: %s->%s\n", original_non_tree_edges[i]->tail->owner->name.c_str(), original_non_tree_edges[i]->head->owner->name.c_str());
            }
        }
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
            for (auto p : n->pins)
            {
                string in_or_out = p->type == pin_type_t::in ? "in" : "out";
                ss << "auto pin_" << node_name << "_" << in_or_out << pin_index << " = node_" << node_name << "->add_pin(pin_type_t::" << in_or_out << ");" << endl;
                pin_index ++;
            }
            ss << endl;
        }
        auto find_pin_index = [](node_t* n, pin_t* p)
        {
            for (int i = 0; i < n->pins.size(); i++)
            {
                if (n->pins[i] == p) return i;
            }
            return -1;
        };
        for (auto [k, edge] : edges)
        {
            auto tail_name = edge->tail->owner->name;
            auto head_name = edge->head->owner->name;

            replace(tail_name.begin(), tail_name.end(), ' ', '_');
            replace(head_name.begin(), head_name.end(), ' ', '_');
            ss << "g.add_edge(" << "pin_" << tail_name << "_out" << find_pin_index(edge->tail->owner, edge->tail)
                << ", " << "pin_" << head_name << "_in" << find_pin_index(edge->head->owner, edge->head) << ");" << endl;
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
        if (min_ranking_dummy_node)
        {
            stack.push_back(min_ranking_dummy_node);
        }
        else
        {
            if (max_ranking_dummy_node)
            {
                stack.push_back(max_ranking_dummy_node);
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

    void tree_t::update_non_tree_edges(set<edge_t*> all_edges)
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
        g.add_edge(pin_K2Node_AddComponent_2_out8, pin_K2Node_CallFunction_14_in1);
        g.add_edge(pin_K2Node_AddComponent_4_out8, pin_K2Node_CallFunction_16_in1);
        g.add_edge(pin_K2Node_AddComponent_4_out8, pin_K2Node_CallFunction_13_in1);
        g.add_edge(pin_K2Node_CallFunction_16_out5, pin_K2Node_CallFunction_18_in2);
        g.add_edge(pin_K2Node_CallFunction_13_out3, pin_K2Node_CallFunction_16_in0);
        g.add_edge(pin_K2Node_CallFunction_14_out3, pin_K2Node_CallFunction_18_in0);
        g.add_edge(pin_K2Node_SwitchEnum_0_out3, pin_K2Node_AddComponent_4_in0);

        g.acyclic();
        set<edge_t*> non_tree_edges;
        g.rank();
    }
}
