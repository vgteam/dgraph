//
//  dgraph.hpp
//

#ifndef dgraph_hpp
#define dgraph_hpp

#include <cstdio>
#include <cstdint>
#include <vector>
#include <utility>
#include <functional>
#include "path.hpp"
#include "handle_types.hpp"
#include "handle_helper.hpp"
#include "dynamic.hpp"

namespace dankgraph {


class graph_t {
        
public:
    graph_t();
    ~graph_t();
        
    /// Look up the handle for the node with the given ID in the given orientation
    handle_t get_handle(const id_t& node_id, bool is_reverse = false) const;
    
    /// Get the ID from a handle
    id_t get_id(const handle_t& handle) const;
    
    /// Get the orientation of a handle
    bool get_is_reverse(const handle_t& handle) const;
    
    /// Invert the orientation of a handle (potentially without getting its ID)
    handle_t flip(const handle_t& handle) const;
    
    /// Get the length of a node
    size_t get_length(const handle_t& handle) const;
    
    /// Get the sequence of a node, presented in the handle's local forward orientation.
    std::string get_sequence(const handle_t& handle) const;
    
    /// Loop over all the handles to next/previous (right/left) nodes. Passes
    /// them to a callback which returns false to stop iterating and true to
    /// continue. Returns true if we finished and false if we stopped early.
    bool follow_edges(const handle_t& handle, bool go_left, const std::function<bool(const handle_t&)>& iteratee) const;
    
    /// Loop over all the nodes in the graph in their local forward
    /// orientations, in their internal stored order. Stop if the iteratee
    /// returns false. Can be told to run in parallel, in which case stopping
    /// after a false return value is on a best-effort basis and iteration
    /// order is not defined.
    void for_each_handle(const std::function<bool(const handle_t&)>& iteratee, bool parallel = false) const;
    
    /// Return the number of nodes in the graph
    /// TODO: can't be node_count because XG has a field named node_count.
    size_t node_size(void) const;
    
    /// Return the smallest ID in the graph, or some smaller number if the
    /// smallest ID is unavailable. Return value is unspecified if the graph is empty.
    id_t min_node_id(void) const;
    
    /// Return the largest ID in the graph, or some larger number if the
    /// largest ID is unavailable. Return value is unspecified if the graph is empty.
    id_t max_node_id(void) const;
    
    ////////////////////////////////////////////////////////////////////////////
    // Interface that needs to be using'd
    ////////////////////////////////////////////////////////////////////////////
    
    /// Loop over all the handles to next/previous (right/left) nodes. Works
    /// with a callback that just takes all the handles and returns void.
    /// Has to be a template because we can't overload on the types of std::function arguments otherwise.
    /// MUST be pulled into implementing classes with `using` in order to work!
    template <typename T>
    auto follow_edges(const handle_t& handle, bool go_left, T&& iteratee) const
        -> typename std::enable_if<std::is_void<decltype(iteratee(get_handle(0, false)))>::value>::type {
        // Implementation only for void-returning iteratees
        // We ought to just overload on the std::function but that's not allowed until C++14.
        // See <https://stackoverflow.com/q/13811180>
        
        // We also can't use result_of<T(handle_t)>::type to sniff the return
        // type out because that ::type would not exist (since that's what you
        // get for a void apparently?) and we couldn't check if it's bool or
        // void.
        
        // So we do this nonsense thing with a trailing return type (to get the
        // actual arg into scope) and a decltype (which is allowed to resolve to
        // void) and is_void (which is allowed to take void) and a fake
        // get_handle call (which is the shortest handle_t-typed expression I
        // could think of).
        
        // Make a wrapper that puts a bool return type on.
        std::function<bool(const handle_t&)> lambda = [&](const handle_t& found) {
            iteratee(found);
            return true;
        };
        
        // Use that
        follow_edges(handle, go_left, lambda);
        
        // During development I managed to get earlier versions of this template to build infinitely recursive functions.
        static_assert(!std::is_void<decltype(lambda(get_handle(0, false)))>::value, "can't take our own lambda");
    }
    
    /// Loop over all the nodes in the graph in their local forward
    /// orientations, in their internal stored order. Works with void-returning iteratees.
    /// MUST be pulled into implementing classes with `using` in order to work!
    template <typename T>
    auto for_each_handle(T&& iteratee, bool parallel = false) const
    -> typename std::enable_if<std::is_void<decltype(iteratee(get_handle(0, false)))>::value>::type {
        // Make a wrapper that puts a bool return type on.
        std::function<bool(const handle_t&)> lambda = [&](const handle_t& found) {
            iteratee(found);
            return true;
        };
        
        // Use that
        for_each_handle(lambda, parallel);
    }

    void for_each_edge(const std::function<bool(const edge_t&)>& iteratee, bool parallel = false){
        for_each_handle([&](const handle_t& handle){
            bool keep_going = true;
            // filter to edges where this node is lower ID or any rightward self-loops
            follow_edges(handle, false, [&](const handle_t& next) {
                if (get_id(handle) <= get_id(next)) {
                        keep_going = iteratee(edge_handle(handle, next));
                    }
                return keep_going;
            });
            if (keep_going) {
                // filter to edges where this node is lower ID or leftward reversing
                // self-loop
                follow_edges(handle, true, [&](const handle_t& prev) {
                    if (get_id(handle) < get_id(prev) ||
                        (get_id(handle) == get_id(prev) && !get_is_reverse(prev))) {
                        keep_going = iteratee(edge_handle(prev, handle));
                    }
                    return keep_going;
                });
            }
        }, parallel);
    };
    
    /// Get a handle from a Visit Protobuf object.
    /// Must be using'd to avoid shadowing.
    //handle_t get_handle(const Visit& visit) const;
    
    ////////////////////////////////////////////////////////////////////////////
    // Additional optional interface with a default implementation
    ////////////////////////////////////////////////////////////////////////////
    
    /// Get the number of edges on the right (go_left = false) or left (go_left
    /// = true) side of the given handle. The default implementation is O(n) in
    /// the number of edges returned, but graph implementations that track this
    /// information more efficiently can override this method.
    size_t get_degree(const handle_t& handle, bool go_left) const;
    
    ////////////////////////////////////////////////////////////////////////////
    // Concrete utility methods
    ////////////////////////////////////////////////////////////////////////////
    
    /// Get a Protobuf Visit from a handle.
    //Visit to_visit(const handle_t& handle) const;
    
    /// Get the locally forward version of a handle
    handle_t forward(const handle_t& handle) const;
    
    /// A pair of handles can be used as an edge. When so used, the handles have a
    /// canonical order and orientation.
    edge_t edge_handle(const handle_t& left, const handle_t& right) const;
    
    /// Such a pair can be viewed from either inward end handle and produce the
    /// outward handle you would arrive at.
    handle_t traverse_edge_handle(const edge_t& edge, const handle_t& left) const;
    
/**
 * This is the interface for a handle graph that stores embedded paths.
 */
    
    ////////////////////////////////////////////////////////////////////////////
    // Path handle interface that needs to be implemented
    ////////////////////////////////////////////////////////////////////////////
    
    /// Determine if a path name exists and is legal to get a path handle for.
    bool has_path(const std::string& path_name) const;
    
    /// Look up the path handle for the given path name.
    /// The path with that name must exist.
    path_handle_t get_path_handle(const std::string& path_name) const;
    
    /// Look up the name of a path from a handle to it
    std::string get_path_name(const path_handle_t& path_handle) const;
    
    /// Returns the number of node occurrences in the path
    size_t get_occurrence_count(const path_handle_t& path_handle) const;

    /// Returns the number of paths stored in the graph
    size_t get_path_count() const;
    
    /// Execute a function on each path in the graph
    // TODO: allow stopping early?
    void for_each_path_handle(const std::function<void(const path_handle_t&)>& iteratee) const;
    
    /// Get a node handle (node ID and orientation) from a handle to an occurrence on a path
    handle_t get_occurrence(const occurrence_handle_t& occurrence_handle) const;
    
    /// Get a handle to the first occurrence in a path.
    /// The path MUST be nonempty.
    occurrence_handle_t get_first_occurrence(const path_handle_t& path_handle) const;
    
    /// Get a handle to the last occurrence in a path
    /// The path MUST be nonempty.
    occurrence_handle_t get_last_occurrence(const path_handle_t& path_handle) const;
    
    /// Returns true if the occurrence is not the last occurence on the path, else false
    bool has_next_occurrence(const occurrence_handle_t& occurrence_handle) const;
    
    /// Returns true if the occurrence is not the first occurence on the path, else false
    bool has_previous_occurrence(const occurrence_handle_t& occurrence_handle) const;
    
    /// Returns a handle to the next occurrence on the path
    occurrence_handle_t get_next_occurrence(const occurrence_handle_t& occurrence_handle) const;
    
    /// Returns a handle to the previous occurrence on the path
    occurrence_handle_t get_previous_occurrence(const occurrence_handle_t& occurrence_handle) const;
    
    /// Returns a handle to the path that an occurrence is on
    path_handle_t get_path_handle_of_occurrence(const occurrence_handle_t& occurrence_handle) const;
    
    /// Returns the 0-based ordinal rank of a occurrence on a path
    size_t get_ordinal_rank_of_occurrence(const occurrence_handle_t& occurrence_handle) const;

    ////////////////////////////////////////////////////////////////////////////
    // Additional optional interface with a default implementation
    ////////////////////////////////////////////////////////////////////////////

    /// Returns true if the given path is empty, and false otherwise
    bool is_empty(const path_handle_t& path_handle) const;

    ////////////////////////////////////////////////////////////////////////////
    // Concrete utility methods
    ////////////////////////////////////////////////////////////////////////////

    /// Loop over all the occurrences along a path, from first through last
    void for_each_occurrence_in_path(const path_handle_t& path, const std::function<void(const occurrence_handle_t&)>& iteratee) const;

/**
 * This is the interface for a handle graph that supports modification.
 */
    /*
     * Note: All operations may invalidate path handles and occurrence handles.
     */
    
    /// Create a new node with the given sequence and return the handle.
    handle_t create_handle(const std::string& sequence);

    /// Create a new node with the given id and sequence, then return the handle.
    handle_t create_handle(const std::string& sequence, const id_t& id);
    
    /// Remove the node belonging to the given handle and all of its edges.
    /// Does not update any stored paths.
    /// Invalidates the destroyed handle.
    /// May be called during serial for_each_handle iteration **ONLY** on the node being iterated.
    /// May **NOT** be called during parallel for_each_handle iteration.
    /// May **NOT** be called on the node from which edges are being followed during follow_edges.
    void destroy_handle(const handle_t& handle);
    
    /// Create an edge connecting the given handles in the given order and orientations.
    /// Ignores existing edges.
    void create_edge(const handle_t& left, const handle_t& right);
    
    /// Convenient wrapper for create_edge.
    inline void create_edge(const edge_t& edge) {
        create_edge(edge.first, edge.second);
    }
    
    /// Remove the edge connecting the given handles in the given order and orientations.
    /// Ignores nonexistent edges.
    /// Does not update any stored paths.
    void destroy_edge(const handle_t& left, const handle_t& right);
    
    /// Convenient wrapper for destroy_edge.
    inline void destroy_edge(const edge_t& edge) {
        destroy_edge(edge.first, edge.second);
    }
    
    /// Remove all nodes and edges. Does not update any stored paths.
    void clear(void);
    
    /// Swap the nodes corresponding to the given handles, in the ordering used
    /// by for_each_handle when looping over the graph. Other handles to the
    /// nodes being swapped must not be invalidated. If a swap is made while
    /// for_each_handle is running, it affects the order of the handles
    /// traversed during the current traversal (so swapping an already seen
    /// handle to a later handle's position will make the seen handle be visited
    /// again and the later handle not be visited at all).
    void swap_handles(const handle_t& a, const handle_t& b);
    
    /// Alter the node that the given handle corresponds to so the orientation
    /// indicated by the handle becomes the node's local forward orientation.
    /// Rewrites all edges pointing to the node and the node's sequence to
    /// reflect this. Invalidates all handles to the node (including the one
    /// passed). Returns a new, valid handle to the node in its new forward
    /// orientation. Note that it is possible for the node's ID to change.
    /// Does not update any stored paths. May change the ordering of the underlying
    /// graph.
    handle_t apply_orientation(const handle_t& handle);
    
    /// Split a handle's underlying node at the given offsets in the handle's
    /// orientation. Returns all of the handles to the parts. Other handles to
    /// the node being split may be invalidated. The split pieces stay in the
    /// same local forward orientation as the original node, but the returned
    /// handles come in the order and orientation appropriate for the handle
    /// passed in.
    /// Updates stored paths.
    std::vector<handle_t> divide_handle(const handle_t& handle, const std::vector<size_t>& offsets);
    
    /// Specialization of divide_handle for a single division point
    inline std::pair<handle_t, handle_t> divide_handle(const handle_t& handle, size_t offset) {
        auto parts = divide_handle(handle, std::vector<size_t>{offset});
        return std::make_pair(parts.front(), parts.back());
    }

/**
 * This is the interface for a handle graph with embedded paths where the paths can be modified.
 * Note that if the *graph* can also be modified, the implementation will also
 * need to inherit from MutableHandleGraph, via the combination
 * MutablePathMutableHandleGraph interface.
 * TODO: This is a very limited interface at the moment. It will probably need to be extended.
 */
    
    /**
     * Destroy the given path. Invalidates handles to the path and its node occurrences.
     */
    void destroy_path(const path_handle_t& path);

    /**
     * Create a path with the given name. The caller must ensure that no path
     * with the given name exists already, or the behavior is undefined.
     * Returns a handle to the created empty path. Handles to other paths must
     * remain valid.
     */
    path_handle_t create_path_handle(const std::string& name);
    
    /**
     * Append a visit to a node to the given path. Returns a handle to the new
     * final occurrence on the path which is appended. Handles to prior
     * occurrences on the path, and to other paths, must remain valid.
     */
    occurrence_handle_t append_occurrence(const path_handle_t& path, const handle_t& to_append);

/// These are the backing data structures that we use to fulfill the above functions

private:

    /// Records node ids to allow for random access and random order
    /// Use the special value "0" to indicate deleted nodes
    dyn::wt_string<dyn::suc_bv> graph_id_wt;
    id_t _max_node_id = 0;
    id_t _min_node_id = 0;

    /// Records edges of the 3' end on the forward strand, delimited by 0
    dyn::wt_string<dyn::suc_bv> edge_fwd_wt;

    /// Marks inverting edges in edge_fwd_wt
    dyn::suc_bv edge_fwd_inv_bv;

    /// Records edges of the 3' end on the reverse strand, delimited by 0
    dyn::wt_string<dyn::suc_bv> edge_rev_wt;

    /// Marks inverting edges in edge_rev_wt
    dyn::suc_bv edge_rev_inv_bv;

    /// Encodes all of the sequences of all nodes and all paths in the graph.
    /// The node sequences occur in the same order as in graph_iv;
    dyn::wt_string<dyn::suc_bv> seq_wt;

    /// Same length as seq_wt. 1's indicate the beginning of a node's sequence.
    dyn::suc_bv boundary_bv;

    /// Same length as seq_wt. 0's indicate that a base is still in the public graph.
    /// 1's indicate that this base has been deleted from the public topology of the graph.
    /// 2's indicate that all nodes or paths that touch this base have been deleted,
    /// and it may be collected in the next compaction cycle.
    dyn::wt_string<dyn::rle_str> dead_wt;

    /// Ordered across the bases in seq_wt, stores the path ids (1-based) at each
    /// segment in seq_wt, delimited by 0
    dyn::wt_string<dyn::suc_bv> path_id_wt;

    /// Stores the path step ranks at each segment in seq_wt, delemited by 0
    /// Note that these can be redundant, in the case of a node division.
    dyn::wt_string<dyn::suc_bv> path_rank_wt;

    /// Stores path names in their internal order, delimited by '$'
    dyn::wt_fmi path_name_fmi;

    /// Marks the beginning of each path name
    dyn::suc_bv path_name_bv;

    /// Encodes the embedded paths of the graph. Each path is represented as three vectors
    /// starts, lengths, orientations
    /// The values in starts correspond to the 0-based indexes of an interval in seq_iv.
    /// The values in lengths are simply the length.
    /// The strand of this interval is given by the corresponding bit in orientations, with 1
    /// indicating reverse strand.
    std::vector<path_t> paths;

    /// A helper to record the number of live nodes
    uint64_t _node_count = 0;
    /// A helper to record the number of live edges
    uint64_t _edge_count = 0;
    /// A helper to record the number of live paths
    uint64_t _path_count = 0;

};

} // end dankness

#endif /* dgraph_hpp */
