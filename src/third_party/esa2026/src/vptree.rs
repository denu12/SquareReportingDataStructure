use acap::{NearestNeighbors, Proximity, knn::Neighborhood, vp::FlatVpTree};

use crate::{
    Embedding, NodeId, Query,
    dvec::DVec,
    query::{self, Graph, Position, SpatialIndex, Update},
};

#[derive(Clone, Debug)]
pub struct DataPoint<const D: usize> {
    index: usize,
    position: DVec<D>,
}

impl<const D: usize> acap::Proximity for DataPoint<D> {
    type Distance = f64;

    fn distance(&self, other: &Self) -> Self::Distance {
        self.position.distance(&other.position)
    }
}

struct RadiusOutput<'q, 'o, const D: usize> {
    target: &'q DataPoint<D>,
    radius: f64,
    results: &'o mut Vec<NodeId>,
}
impl<'q, 'o, 'v, const D: usize> Neighborhood<&'q DataPoint<D>, &'v DataPoint<D>>
    for RadiusOutput<'q, 'o, D> {
    fn target(&self) -> &'q DataPoint<D> { self.target }
    fn contains<T: PartialOrd<f64>>(&self, distance: T) -> bool { distance <= self.radius }
    fn consider(&mut self, item: &'v DataPoint<D>) -> f64 {
        let distance = self.target.distance(item);
        if distance <= self.radius { self.results.push(item.index); }
        distance
    }
}

pub struct VPTree<'a, const D: usize> {
    pub positions: Vec<DVec<D>>,
    pub graph: &'a crate::graph::Graph,
    pub vptree: FlatVpTree<DataPoint<D>>,
}

impl<'a, const D: usize> Clone for VPTree<'a, D> {
    fn clone(&self) -> Self {
        let data = self
            .positions
            .iter()
            .enumerate()
            .map(|(i, pos)| DataPoint {
                index: i,
                position: *pos,
            })
            .collect::<Vec<_>>();
        let vptree = FlatVpTree::balanced(data);
        Self {
            positions: self.positions.clone(),
            graph: self.graph,
            vptree,
        }
    }
}

impl<'a, const D: usize> VPTree<'a, D> {
    pub fn new(embedding: Embedding<'a, D>) -> Self {
        let positions = embedding.positions.clone();
        let data_points = positions
            .iter()
            .enumerate()
            .map(|(i, pos)| DataPoint {
                index: i,
                position: *pos,
            })
            .collect::<Vec<_>>();
        let vptree = FlatVpTree::balanced(data_points);
        Self {
            positions,
            graph: embedding.graph,
            vptree,
        }
    }
}

impl<'a, const D: usize> Graph for VPTree<'a, D> {
    fn is_connected(&self, first: NodeId, second: NodeId) -> bool {
        self.graph.is_connected(first, second)
    }

    fn neighbors(&self, index: NodeId) -> &[NodeId] {
        self.graph.neighbors(index)
    }

    fn weight(&self, index: NodeId) -> f64 {
        self.graph.weight(index)
    }
}

impl<'a, const D: usize> Position<D> for VPTree<'a, D> {
    fn position(&self, index: NodeId) -> &DVec<D> {
        &self.positions[index]
    }

    fn num_nodes(&self) -> usize {
        self.positions.len()
    }
}

impl<'a, const D: usize> Update<D> for VPTree<'a, D> {
    fn update_positions(&mut self, positions: &[DVec<D>], _: Option<f64>) {
        self.positions = positions.to_vec();
        let data_points = self
            .positions
            .iter()
            .enumerate()
            .map(|(i, pos)| DataPoint {
                index: i,
                position: *pos,
            })
            .collect::<Vec<_>>();
        self.vptree = FlatVpTree::balanced(data_points);
    }
}

impl<'a, const D: usize> Query<D> for VPTree<'a, D> {
    fn query_radius(&self, pos: DVec<D>, radius: f64, results: &mut Vec<NodeId>) {
        let query_point = DataPoint {
            index: usize::MAX,
            position: pos,
        };

        // Public visitor API: fixed-radius reporting needs no kNN heap or sort.
        self.vptree.search(RadiusOutput {
            target: &query_point, radius, results,
        });
    }
}

impl<'a, const D: usize> SpatialIndex<D> for VPTree<'a, D> {
    fn name(&self) -> String {
        "vptree".to_string()
    }

    fn implementation_string(&self) -> &'static str {
        include_str!("vptree.rs")
    }
}

impl<'a, const D: usize> query::Embedder<'a, D> for VPTree<'a, D> {
    fn new(embedding: &crate::Embedding<'a, D>) -> Self {
        Self::new(embedding.clone())
    }
}
