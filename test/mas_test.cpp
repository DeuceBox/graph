//            Copyright Fernando Vilas 2012.
//     Based on stoer_wagner_test.cpp by Daniel Trebbien.
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE_1_0.txt or the copy at
//         http://www.boost.org/LICENSE_1_0.txt)

#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/exception.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/read_dimacs.hpp>
#include <boost/graph/maximum_adjacency_search.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/graph/property_maps/constant_property_map.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/tuple/tuple_io.hpp>

#include <boost/graph/iteration_macros.hpp>

typedef boost::adjacency_list< boost::vecS, boost::vecS, boost::undirectedS,
    boost::no_property, boost::property< boost::edge_weight_t, int > >
    undirected_graph;
typedef boost::property_map< undirected_graph, boost::edge_weight_t >::type
    weight_map_type;
typedef boost::property_traits< weight_map_type >::value_type weight_type;

typedef boost::adjacency_list< boost::vecS, boost::vecS, boost::undirectedS >
    undirected_unweighted_graph;

std::string test_dir;

struct edge_t
{
    unsigned long first;
    unsigned long second;
};

template < typename Graph, typename KeyedUpdatablePriorityQueue >
class mas_test_visitor : public boost::default_mas_visitor
{
public:
    typedef typename boost::graph_traits< Graph >::vertex_descriptor
        vertex_descriptor;
    typedef typename KeyedUpdatablePriorityQueue::key_type weight_type;

    explicit mas_test_visitor(KeyedUpdatablePriorityQueue& pq)
    :   m_pq_(pq),
        vertex_visit_order_(std::make_shared<std::vector<vertex_descriptor>>()),
        vertex_weights_when_visited_(std::make_shared<std::vector<weight_type>>())
    {}

    void clear()
    {
        vertex_visit_order_->clear();
        vertex_weights_when_visited_->clear();
    }

    void start_vertex(vertex_descriptor u, const Graph& g)
    {
        vertex_visit_order_->push_back(u);

        const weight_type u_weight = get(m_pq_.keys(), u);
        vertex_weights_when_visited_->push_back(u_weight);
    }

    const std::vector<vertex_descriptor>& vertex_visit_order() const {
        return *vertex_visit_order_;
    }

    const std::vector<weight_type>& vertex_weights_when_visited() const {
        return *vertex_weights_when_visited_;
    }

private:
    const KeyedUpdatablePriorityQueue& m_pq_;
    std::shared_ptr<std::vector<vertex_descriptor>> vertex_visit_order_;
    std::shared_ptr<std::vector<weight_type>> vertex_weights_when_visited_;
};

// the example from Stoer & Wagner (1997)
// Check various implementations of the ArgPack where
// the weights are provided in it, and one case where
// they are not.
void test0()
{
    typedef boost::graph_traits< undirected_graph >::vertex_descriptor
        vertex_descriptor;
    typedef boost::graph_traits< undirected_graph >::edge_descriptor
        edge_descriptor;

    std::array< edge_t, 12 > edge_list = { { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 0, 4 }, { 1, 4 }, { 1, 5 },
            { 2, 6 }, { 3, 6 }, { 3, 7 }, { 4, 5 }, { 5, 6 }, { 6, 7 } } };
    const std::array<weight_type, 12> ws = { 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 1, 3 };
    const std::size_t vertices_count = 8;

    undirected_graph g(edge_list.cbegin(), edge_list.cend(), ws.cbegin(), vertices_count, ws.size());

    weight_map_type weights = get(boost::edge_weight, g);

    std::map< vertex_descriptor, vertex_descriptor > assignment;
    boost::associative_property_map<
        std::map< vertex_descriptor, vertex_descriptor > >
        assignments(assignment);

    typedef boost::shared_array_property_map< weight_type,
        boost::property_map< undirected_graph,
            boost::vertex_index_t >::const_type >
        distances_type;
    distances_type distances = boost::make_shared_array_property_map(
        num_vertices(g), weight_type(0), get(boost::vertex_index, g));
    typedef std::vector< vertex_descriptor >::size_type index_in_heap_type;
    typedef boost::shared_array_property_map< index_in_heap_type,
        boost::property_map< undirected_graph,
            boost::vertex_index_t >::const_type >
        indicesInHeap_type;
    indicesInHeap_type indicesInHeap = boost::make_shared_array_property_map(
        num_vertices(g), index_in_heap_type(-1), get(boost::vertex_index, g));
    boost::d_ary_heap_indirect< vertex_descriptor, 22, indicesInHeap_type,
        distances_type, std::greater< weight_type > >
        pq(distances, indicesInHeap);

    mas_test_visitor< undirected_graph,
        boost::d_ary_heap_indirect< vertex_descriptor, 22, indicesInHeap_type,
            distances_type, std::greater< weight_type > > >
        test_vis(pq);

    boost::maximum_adjacency_search(g,
        boost::weight_map(weights)
            .visitor(test_vis)
            .root_vertex(*vertices(g).first)
            .vertex_assignment_map(assignments)
            .max_priority_queue(pq));

    const std::vector<vertex_descriptor> expected_vertex_order1 = { 0, 4, 1, 5, 2, 3, 6, 7 };
    const std::vector<weight_type> expected_weights_when_visited1 = { 9, 3, 4, 5, 3, 4, 5, 5 };

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_visit_order().cbegin(),
        test_vis.vertex_visit_order().cend(),
        expected_vertex_order1.cbegin(),
        expected_vertex_order1.cend()
    );

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_weights_when_visited().cbegin(),
        test_vis.vertex_weights_when_visited().cend(),
        expected_weights_when_visited1.cbegin(),
        expected_weights_when_visited1.cend()
    );

    test_vis.clear();

    boost::maximum_adjacency_search(g,
        boost::weight_map(weights)
            .visitor(test_vis)
            .root_vertex(*vertices(g).first)
            .max_priority_queue(pq));

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_visit_order().cbegin(),
        test_vis.vertex_visit_order().cend(),
        expected_vertex_order1.cbegin(),
        expected_vertex_order1.cend()
    );

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_weights_when_visited().cbegin(),
        test_vis.vertex_weights_when_visited().cend(),
        expected_weights_when_visited1.cbegin(),
        expected_weights_when_visited1.cend()
    );

    test_vis.clear();

    boost::maximum_adjacency_search(
        g, boost::weight_map(weights).visitor(test_vis).max_priority_queue(pq));

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_visit_order().cbegin(),
        test_vis.vertex_visit_order().cend(),
        expected_vertex_order1.cbegin(),
        expected_vertex_order1.cend()
    );

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_weights_when_visited().cbegin(),
        test_vis.vertex_weights_when_visited().cend(),
        expected_weights_when_visited1.cbegin(),
        expected_weights_when_visited1.cend()
    );

    test_vis.clear();

    boost::maximum_adjacency_search(g,
        boost::weight_map(weights).visitor(
            boost::make_mas_visitor(boost::null_visitor())));

    boost::maximum_adjacency_search(g, boost::weight_map(weights));

    boost::maximum_adjacency_search(g, boost::root_vertex(*vertices(g).first));

    test_vis.clear();
    boost::maximum_adjacency_search(g,
        boost::weight_map(
            boost::make_constant_property< edge_descriptor >(weight_type(1)))
            .visitor(test_vis)
            .max_priority_queue(pq));

    const std::vector<vertex_descriptor> expected_vertex_order2 = { 0, 1, 4, 5, 2, 6, 3, 7 };
    const std::vector<weight_type> expected_weights_when_visited2 = { 9, 1, 2, 2, 1, 2, 2, 2 };

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_visit_order().cbegin(),
        test_vis.vertex_visit_order().cend(),
        expected_vertex_order2.cbegin(),
        expected_vertex_order2.cend()
    );

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_weights_when_visited().cbegin(),
        test_vis.vertex_weights_when_visited().cend(),
        expected_weights_when_visited2.cbegin(),
        expected_weights_when_visited2.cend()
    );
}

// Check the unweighted case
// with and without providing a weight_map
void test1()
{
    typedef boost::graph_traits<
        undirected_unweighted_graph >::vertex_descriptor vertex_descriptor;
    typedef boost::graph_traits< undirected_unweighted_graph >::edge_descriptor
        edge_descriptor;

    std::array< edge_t, 12 > edge_list = { { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 0, 4 }, { 1, 4 }, { 1, 5 },
            { 2, 6 }, { 3, 6 }, { 3, 7 }, { 4, 5 }, { 5, 6 }, { 6, 7 } } };
    const std::size_t vertices_count = 8;

    undirected_unweighted_graph g(edge_list.cbegin(), edge_list.cend(), vertices_count);

    std::map< vertex_descriptor, vertex_descriptor > assignment;
    boost::associative_property_map<
        std::map< vertex_descriptor, vertex_descriptor > >
        assignments(assignment);

    typedef unsigned weight_type; // weight_type corresponds to the priority in the priority queue
    typedef boost::shared_array_property_map< weight_type,
        boost::property_map< undirected_graph,
            boost::vertex_index_t >::const_type >
        distances_type;
    distances_type distances = boost::make_shared_array_property_map(
        num_vertices(g), weight_type(0), get(boost::vertex_index, g));
    typedef std::vector< vertex_descriptor >::size_type index_in_heap_type;
    typedef boost::shared_array_property_map< index_in_heap_type,
        boost::property_map< undirected_graph,
            boost::vertex_index_t >::const_type >
        indicesInHeap_type;
    indicesInHeap_type indicesInHeap = boost::make_shared_array_property_map(
        num_vertices(g), index_in_heap_type(-1), get(boost::vertex_index, g));
    boost::d_ary_heap_indirect< vertex_descriptor, 22, indicesInHeap_type,
        distances_type, std::greater< weight_type > >
        pq(distances, indicesInHeap);

    mas_test_visitor< undirected_unweighted_graph,
        boost::d_ary_heap_indirect< vertex_descriptor, 22, indicesInHeap_type,
            distances_type, std::greater< weight_type > > >
        test_vis(pq);

    boost::maximum_adjacency_search(g,
        boost::weight_map(
            boost::make_constant_property< edge_descriptor >(weight_type(1)))
            .visitor(test_vis)
            .max_priority_queue(pq));

    const std::vector<vertex_descriptor> expected_vertex_order1 = { 0, 1, 4, 5, 2, 6, 3, 7 };
    const std::vector<weight_type> expected_weights_when_visited1 = { 9, 1, 2, 2, 1, 2, 2, 2 };

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_visit_order().cbegin(),
        test_vis.vertex_visit_order().cend(),
        expected_vertex_order1.cbegin(),
        expected_vertex_order1.cend()
    );

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_weights_when_visited().cbegin(),
        test_vis.vertex_weights_when_visited().cend(),
        expected_weights_when_visited1.cbegin(),
        expected_weights_when_visited1.cend()
    );

    test_vis.clear();

    const std::array<weight_type, 12> ws = { 2, 3, 4, 3, 2, 2, 2, 2, 2, 3, 1, 3 };
    std::map< edge_descriptor, weight_type > wm;

    weight_type i = 0;
    BGL_FORALL_EDGES(e, g, undirected_unweighted_graph)
    {
        wm[e] = ws[i];
        ++i;
    }
    boost::associative_property_map< std::map< edge_descriptor, weight_type > >
        ws_map(wm);

    boost::maximum_adjacency_search(
        g, boost::weight_map(ws_map).visitor(test_vis).max_priority_queue(pq));
    
    const std::vector<vertex_descriptor> expected_vertex_order2 = { 0, 4, 1, 5, 2, 3, 6, 7 };
    const std::vector<weight_type> expected_weights_when_visited2 = { 9, 3, 4, 5, 3, 4, 5, 5 };

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_visit_order().cbegin(),
        test_vis.vertex_visit_order().cend(),
        expected_vertex_order2.cbegin(),
        expected_vertex_order2.cend()
    );

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_weights_when_visited().cbegin(),
        test_vis.vertex_weights_when_visited().cend(),
        expected_weights_when_visited2.cbegin(),
        expected_weights_when_visited2.cend()
    );
}

using mas_test_vertex_descriptor = boost::graph_traits< undirected_unweighted_graph >::vertex_descriptor;
using mas_test_edge_descriptor = boost::graph_traits< undirected_unweighted_graph >::edge_descriptor;

using mas_test_weight_type = std::size_t;
using mas_test_distances_type = boost::shared_array_property_map< mas_test_weight_type, boost::property_map< undirected_graph, boost::vertex_index_t >::const_type >;
using mas_test_index_in_heap_type = std::vector< mas_test_vertex_descriptor >::size_type;
using mas_test_indicesInHeap_type = boost::shared_array_property_map< mas_test_index_in_heap_type, boost::property_map< undirected_graph, boost::vertex_index_t >::const_type >;
constexpr std::size_t mas_test_arity = 4;
using mas_test_maxheap_type = boost::d_ary_heap_indirect< mas_test_vertex_descriptor, mas_test_arity, mas_test_indicesInHeap_type, mas_test_distances_type, std::greater< mas_test_weight_type > >;
using mas_text_visitor_type = mas_test_visitor< undirected_unweighted_graph, mas_test_maxheap_type>;

template <typename Graph>
mas_test_maxheap_type create_mas_test_maxheap(const Graph& g) {
    mas_test_distances_type distances = boost::make_shared_array_property_map(
        num_vertices(g), mas_test_weight_type(0), get(boost::vertex_index, g));

    mas_test_indicesInHeap_type indicesInHeap = boost::make_shared_array_property_map(
        num_vertices(g), mas_test_index_in_heap_type(-1), get(boost::vertex_index, g));

    return mas_test_maxheap_type(distances, indicesInHeap);
}

template <std::size_t edge_count, std::size_t vertices_count>
void test_unweighted(const std::array<edge_t, edge_count>& edge_list, const std::array<mas_test_vertex_descriptor, vertices_count>& expected_vertex_order, const std::array<mas_test_weight_type, vertices_count>& expected_weights_when_visited) {
    const undirected_unweighted_graph g(edge_list.cbegin(), edge_list.cend(), vertices_count);

    mas_test_maxheap_type pq = create_mas_test_maxheap(g);
    mas_text_visitor_type test_vis = mas_text_visitor_type(pq);

    boost::maximum_adjacency_search(
        g,
        boost::weight_map(
            boost::make_constant_property< mas_test_edge_descriptor >(mas_test_weight_type(1)))
            .visitor(test_vis)
            .max_priority_queue(pq)
    );

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_visit_order().cbegin(),
        test_vis.vertex_visit_order().cend(),
        expected_vertex_order.cbegin(),
        expected_vertex_order.cend()
    );

    BOOST_TEST_ALL_EQ(
        test_vis.vertex_weights_when_visited().cbegin(),
        test_vis.vertex_weights_when_visited().cend(),
        expected_weights_when_visited.cbegin(),
        expected_weights_when_visited.cend()
    );
}

void test2_noweights() {
    constexpr std::size_t edge_count = 1;
    constexpr std::size_t vertices_count = 2;

    const std::array< edge_t, edge_count > edge_list = { { { 0, 1 } } };

    const std::array< mas_test_vertex_descriptor, vertices_count > expected_vertex_order = { 0, 1 };
    const std::array< mas_test_weight_type, vertices_count > expected_weights_when_visited = { vertices_count+1, 1 };

    test_unweighted(
        edge_list,
        expected_vertex_order,
        expected_weights_when_visited
    );
}

void test3_noweights() {
    constexpr std::size_t edge_count = 2;
    constexpr std::size_t vertices_count = 3;

    const std::array< edge_t, edge_count > edge_list = { { { 0, 1 }, { 1, 2 } } };

    const std::array< mas_test_vertex_descriptor, vertices_count > expected_vertex_order = { 0, 1, 2 };
    const std::array< mas_test_weight_type, vertices_count > expected_weights_when_visited = { vertices_count+1, 1, 1 };

    test_unweighted(
        edge_list,
        expected_vertex_order,
        expected_weights_when_visited
    );
}

void test4_noweights() {
    constexpr std::size_t edge_count = 3;
    constexpr std::size_t vertices_count = 3;

    const std::array< edge_t, edge_count > edge_list = { { { 0, 1 }, { 0, 2 }, { 1, 2 } } };

    const std::array< mas_test_vertex_descriptor, vertices_count > expected_vertex_order = { 0, 1, 2 };
    const std::array< mas_test_weight_type, vertices_count > expected_weights_when_visited = { vertices_count+1, 1, 2 };

    test_unweighted(
        edge_list,
        expected_vertex_order,
        expected_weights_when_visited
    );
}

#include <boost/graph/iteration_macros_undef.hpp>

int main(int argc, char* argv[])
{
    if (BOOST_TEST(argc == 2)) {
        test_dir = argv[1];
        test0();
        test1();
        test2_noweights();
        test3_noweights();
        test4_noweights();
    }
    return boost::report_errors();
}
