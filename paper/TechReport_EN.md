# I. In searching of optimal antienthropy algorithm for δ-CmRDT. Work in progress report.
# II. In searching of optimal data causality tracking mechanism. Work in progress report.

### Abstract.
The approach to minimize memory consuption of data causality traching mechanism is proposed. Syncronization chemes that forms partially ordered set of dimention two are explored and the algorithm of checking the poset for being of dimension two is described. There was found and described some general and specific schemes, in wich there is a possibility to make such cauality tracking mechnisms.

## Problem Description.

With dramatically increasing of datastores load (the handling of huge amount of  read/write requests) there appears critical demand to migrate on another database acrhitectures systems, differrent from classical "client-server". [Amazon Dynamo](https://aws.amazon.com/dynamodb/)[26], [Cassandra](http://cassandra.apache.org/) and [Riak KV](http://basho.com/products/riak-kv/) are the best representatives of such databases. Its architecture is focused around _partition tolerance_, _read and write and availability_ and _eventual consistency_.

Those systems lay in the area of heuristic CAP theorem [1](https://en.wikipedia.org/wiki/CAP_theorem), stated that among requirements of _consistency_, _availability_ and _partition tolerance_ only two can be achieved.

Moreover there additional requirements as _massive replication_ appears. To achive the load distribution, decentralization and improvment of avalability and geo-distribution, data store systems became global. Under such conditions the easing of restrictions of cosistensy reqirements is inevitable: from _strong consistensy_ to [eventual consistency](https://en.wikipedia.org/wiki/Eventual_consistency) [2][3].

That systems follow the design, where data always can be written: copies of the same data (replicas) are allowed to be slightly diverge. However, depending on the method of synchronization there might conflicts arise, like occurrence of multiple concurrent copies or loss of updated data.

In order to maintain consistency and replica syncronization ability there different data causality tracking mechinisms are exists. The most common and well-known is [Version Vector](https://en.wikipedia.org/wiki/Version_vector)[4][5], but its size has  asymptotycally lineary dependency function form number of replicas, and therefore can not be used in the massive replicated systems.
This interconnection is discussed in more detail below.

A lot of alternatives have been created recently, partially solves the problem of memory consumptions, some of wich are version vector variations, others uses another approach.

Among them we can be distinguished:

* Dotted Version Vector[[6]](http://gsd.di.uminho.pt/members/vff/dotted-version-vectors-2012.pdf). Perhaps the most practical of all. It is based on the principle of distingushing a special subset of (server) nodes, that directly track causal inforamtion; and on the _dot_ concept, designed to distinguish the omissions in updates made on non-"server" nodes.

* Hash Histories[[7]](http://oceanstore.cs.berkeley.edu/publications/papers/pdf/hh_icdcs03_kang.pdf). Completely stores the execution graph with the nodes containing the hash of the data. On the one hand it contains redundant inforamtion, on the other hand has the ability of detecting equal copies, happened to be so by accident.

* Version stamps[[8]](http://haslab.uminho.pt/cbm/files/10.1.1.16.8235.pdf). Bases on principle of dynamic induction of identifiers for nodes, at the same time serving to distinguish concurrent events from dependent. The value of additional data depend asymptotically lineary form the count of updates and syncronizations.

* These methods belongs to the _exact_, i.e. that who always correctly detects of causal reketion between two copies.

* There are also inaccurate - for which it is not always possible to claim (or to calaim with probability less then one) that are the data is concurrent or causally dependent.

* And there methods dependent on the network topology.

It is also important to note the relationship between causality tracking and logical clocks. The first can be considered as a special case of logical clocks. Therefore, to some extent, the variants of such clocks can be considered in the present context [[9]](https://haslab.wordpress.com/2011/07/08/version-vectors-are-not-vector-clocks/).

Among other things, the causality tracking mechanisms are used in the other fields of science, in particular sociology and social networks researches [[10]](https://arxiv.org/pdf/1304.4058.pdf). And disadvantages, in particular, version vector, are espasially strong.

## CRDT.
Based on the experience of distributed databases with master-master replication and generalizing the methods of data reconsilation in systems with non-strict data consistency there was introdused Confict Free Replicated Datatypes. Use of such structures guarantees strict-evetual consistency [3]. Among other things, CRDT in explicit forms already uses, or are going to use some moders databases: for example Microsoft Cosmos DB [[28]](https://docs.microsoft.com/en-us/azure/cosmos-db/multi-region-writers) and Redis [[27]](http://lp.redislabs.com/rs/915-NFD-128/images/WP-RedisLabs-Redis-Conflict-free-Replicated-Data-Types.pdf).

There are two types of CRDT exists [[23]](https://hal.inria.fr/inria-00555588/document):
* CmRDT - operations oriented CRDT (op-based CRDT, Commmutative RDT). Syncronization in that CRDT is perfomed by kind of atomic broadcast of update operations. Such operations must commute in order to converge to equal state for that structure.
* СvRDT - state oriented (state-based CRDT, Convergent RDT). Syncronization in that CRDT is perfomed by state dissemination, wich is in turn merged in one by perfoming spesial operation. There is exist modification, δ-CRDT, performing replication by dissemination not only full state, but only the modified part.

Each of type has its advandejes and disadvnantages:
+ CmRDT
	+ **+** low traffic rate on opertaions broadcast
	+ **-** requires atomic broadcast type of commutication
	+ **-** dependent of curtain type requires cousal order of opertaions broadcast

+ δ-CvRDT
	+ **+** gossip-based state dissemination protocol
	+ **-** requires additional metainformation to track cousal relations
	+ **-** traffic rate can be higher than for CmRDT

## Paper Scheme.

## Base Definitions.

The set P, with reflecive and transitive binary relations on it, called **preorder**.

**Partial Order**: is a binary relation ≤ over a set P satisfying axioms of reflexivity, antisymmetry and transitivity. Formally, for all ∀ a,b,c ∈ P, it must satisfy:

1. a ≤ a (reflexivity)
2. if a ≤ b and b ≤ a, then a = b (antisymmetry)
3. if a ≤ b and b ≤ c, then a ≤ c (transitivity)

The set P with partial order ≤ on it (P, ≤) is called **Partially Ordered Set** or **poset**.

Further in this arctical for the denotation of partial order the symbol **→** will be used.

For every a and b ∈ (P, →) if a→b or b→a, it is said thar a and be are comparable to each other, otherwise a and b are incomparable.

**Linear order** or **Total order** is a poset, wherein erevy two elements are comparable.
Such poset also called **chain**. Poset in wish every par is pairwise incoamarable is called **antichain**.

**Linear extension** for the poset (P, →) is a total order (L, →), where:
1. L = P, base sets are equal
2. If a→b ∈ (P, →), then a→b ∈ (L, →). I.e. Linear extension must preserve the order.

**Theorem (_Szpilrajn_)**:
For every posed there is exists at least one linear extension[[13]](https://en.wikipedia.org/wiki/Szpilrajn_extension_theorem).

**Width of the poset** is the number of elements in the maximal antichain. Formally:
    X ∈ P where X is antichain, and for eny another Y ∈ P, Y - antichain, and |X|≥|Y| then |X| is the width of the poset P.
    
**Theorem (_Dilworth_)**:
The minimal number of antichains, on which poset P can be partitioned is equal to the number of elements in maximal antichain[[14]](https://en.wikipedia.org/wiki/Dilworth%27s_theorem).

Hence, the another definition of poset width:
It is minimum number of chains, on which poset P can be partitioned.

**Poset height** is the number of elements in the maximal chain. Formally
    X ∈ P where X is chain, for every Y ∈ P, where Y is chain and |X|>=|Y|, then |X| is height of the poset P.
    
**Realizer** of poset (P, →) is a family of linear extensions if their intersection results in original P poset.

**Dimension of the poset** (P, →) is the size of the minimal possible realizer.

The dimansion of the poset (P, →) always denoted by **dim(P)** [[15]](http://www.jstor.org/stable/2371374).

**Theorem (_Hiraguti_)**:
    The dimension of the poset P cant ba larger its width [16].

**Critical pair** of poset elements (a, b) is an ordered pair, satisfying:

* a and b are incomparable.
* for every z ∈ P, if b→z then a→z
* for every z ∈ P, if z→a then z→b

As consequence: if (a, b) is critical pair, then the addition of a→b relation preserve partial order property of →.

Linear extension L is **reversing** critial pair (a, b) if it is true that b→a in L.

**Statement** Any nonempty family R of linear extensions is a realizer of poset (P, →) if and only if, for every critical pair (a, b) b→a for at least one linear order in R.

**Theorem (_Yanakakkis_)** The problem of detection that poset is of dimension more then 2 is NP-hard[[17]](https://www.researchgate.net/publication/230596220_The_Complexity_of_the_Partial_Order_Dimension_Problem).

As a consequence, there polynomial algorithm for detection that poset is of dimension excatly 2 is exists.

**Hypergraph of incomparable pairs H(P)** associated with poset (P,  →) is a hypergraph, in that the vertices are the incomarable pairs in the poset P, and the edges are those sets S of incomparable pairs satisfying:

* No linear extension L of P reverses all incomparable pairs is S.
* If T is a proper subset of S (T ⊂ S), then there is a linear extension L of P wich reverses all incomparable pairs in T.

So the hyperedges of H(P) is that minimal sets S of incomparable pairs, which cannot be reversed in the only one linear extension.

**Graph of incomparable pairs G(P)** is the ordinary subgraph of H(P) consists only of ordinary 2-size edges.

**Hypergraph of incomparable critical pairs H<sub>c</sub>(P)** is a subhypergraph of H(P), induced by critical pairs.

**Graph of incomparable critical pairs G<sub>c</sub>(P)** is a ordinary subgraph of H<sub>c</sub>(P) with the only 2-size edges left.

**Chromatic number χ(H)** of (hyper)graph H is the smallest number k of colors needed to color H.

For that graphs and hypergraphs incompatible and critical pairs it is true that:
**dim(P) == χ(H) == χ(H<sub>c</sub>) ⩾ χ(G) == χ(G<sub>c</sub>)**.

**Theorem (_Felsner_, _Trotter_)**:
If graph G<sub>c</sub>(P) is a graph of incomparabel critial pairs of poset P, which is not a linera order. Then the dimension of P is equal 2 if and only if χ(G) == 2 [[18]](https://link.springer.com/article/10.1023/A:1006429830221).

## Poset Model.

## Poset Dimesion. 

## Poset Сanonical Form.

## Extra Definitions.

## Ψ Isomorphism.

## Searching of Ψ of dimension 2.

## Results

## Future Works

## Bibliography
 1.  _Brewer, Eric A._  [A Certain Freedom: Thoughts on the CAP Theorem](http://portal.acm.org/ft_gateway.cfm?id=1835701&type=pdf&CFID=25475815)  (англ.) // Proceeding of the XXIX ACM SIGACT-SIGOPS symposium on Principles of distributed computing. — N. Y.: [ACM](https://ru.wikipedia.org/wiki/ACM "ACM"), 2010. — Iss. 29. — No. 1. — P. 335—336.
 2. [Vogels, W.](https://en.wikipedia.org/wiki/Werner_Vogels "Werner Vogels")  (2009). "Eventually consistent".  _Communications of the ACM_.  **52**: 40.  [doi](https://en.wikipedia.org/wiki/Digital_object_identifier "Digital object identifier"):[10.1145/1435417.1435432](https://doi.org/10.1145%2F1435417.1435432).
 3. Saito, Yasushi; Shapiro, Marc (2005). "Optimistic replication". _[ACM Computing Surveys](https://en.wikipedia.org/wiki/ACM_Computing_Surveys "ACM Computing Surveys")_. **37** (1): 42–81. [doi](https://en.wikipedia.org/wiki/Digital_object_identifier "Digital object identifier"):[10.1145/1057977.1057980](https://doi.org/10.1145%2F1057977.1057980).
 4. Mattern, Friedman. (October 1988), "Virtual Time and Global States of Distributed Systems", in Cosnard, M., _Proc. Workshop on Parallel and Distributed Algorithms_, Chateau de Bonas, France: Elsevier, pp. 215–226
 5. Colin J. Fidge (February 1988). ["Timestamps in Message-Passing Systems That Preserve the Partial Ordering"](http://zoo.cs.yale.edu/classes/cs426/2012/lab/bib/fidge88timestamps.pdf)  (PDF). In K. Raymond (Ed.). _Proc. of the 11th Australian Computer Science Conference (ACSC'88)
 6. Nuno Preguiça, Carlos Baquero, Paulo Almeida, Victor Fonte and Ricardo Gonçalves. Brief Announcement: Efficient Causality Tracking in Distributed Storage Systems With Dotted Version Vectors. ACM PODC, pp. 335-336, 2012.
 7. ByungHoon Kang, Robert Wilensky, and John Kubiatowicz. The Hash History Approach for Reconciling Mutual Inconsistency. ICDCS, pp. 670-677, IEEE Computer Society, 2003.
 8. Paulo Almeida, Carlos Baquero and Victor Fonte. Version Stamps: Decentralized Version Vectors. ICDCS, pp. 544-551, 2002.
 9. https://haslab.wordpress.com/2011/07/08/version-vectors-are-not-vector-clocks/
 10. Lee, Conrad,  Nick, Bobo,  Brandes, Ulrik,  Cunningham, Pádraig : Link Prediction with Social Vector Clocks. In: Proceedings of the 19th ACM SIGKDD international conference on Knowledge discovery and data mining. ACM, 2013-08-14.
 11. Bernadette Charron-Bost. Concerning the size of logical clocks in distributed systems. Journal Information Processing Letters, Volume 39, Issue 1, July 12, 1991, Pages 11-16.
 12. Leslie Lamport . Time, clocks, and the ordering of events in a distributed system. Comm. ACM 21, 7 (July 1978), 558 - 565.
 13. Szpilrajn, E. (1930), ["Sur l'extension de l'ordre partiel"](http://matwbn.icm.edu.pl/tresc.php?wyd=1&tom=16), _[Fundamenta Mathematicae](https://en.wikipedia.org/wiki/Fundamenta_Mathematicae "Fundamenta Mathematicae")_, **16**: 386–389
 14. [Dilworth, Robert P.](https://en.wikipedia.org/wiki/Robert_P._Dilworth "Robert P. Dilworth") (1950), "A Decomposition Theorem for Partially Ordered Sets", _[Annals of Mathematics](https://en.wikipedia.org/wiki/Annals_of_Mathematics "Annals of Mathematics")_, **51** (1): 161–166
 15. Dushnik, Ben & Miller, E. W. (1941), "[Partially Ordered Sets](https://www.jstor.org/stable/2371374)", _[American Journal of Mathematics](https://ru.wikipedia.org/wiki/American_Journal_of_Mathematics "American Journal of Mathematics")_ Т. 63 (3): 600-610
 16. Tosio Hiragushi, On the dimension of orders, Tech. Rept., University of Kanazawa, 1955.
 17. Mihalis Yannakakis, The Complexity of the Partial Order Dimension Problem, [SIAM Journal on Algebraic and Discrete Methods](https://www.researchgate.net/journal/0196-5212_SIAM_Journal_on_Algebraic_and_Discrete_Methods)3(3):351-358 · September 1982
 18. Stefan Felsner, William T. Trotter. Dimension, Graph and Hypergraph Coloring. June 2000, Volume  17, pp 167–177
 19. Douglas Parker, Gerald Popek, Gerard Rudisin, Allen Stoughton, Bruce Walker, Evelyn Walton, Johanna Chow, David Edwards, Stephen Kiser, and [Charles Kline](https://en.wikipedia.org/w/index.php?title=Charles_S._Kline&action=edit&redlink=1 "Charles S. Kline (page does not exist)"). Detection of mutual inconsistency in distributed systems. Transactions on Software Engineering. 1983
 20. P.A.S. Ward. # An offline algorithm for dimension-bound analysis. Parallel Processing, 1999. Proceedings. 1999 International Conference on. 24-24 Sept. 1999
 21. Paul A. S. Ward. A framework algorithm for dynamic, centralized dimension-bounded timestamps. Proceeding CASCON '00 Proceedings of the 2000 conference of the Centre for Advanced Studies on Collaborative research Page 14
 22. Paul A.S. Ward, David J. Taylor. A Hierarchical Cluster Algorithm for Dynamic, Centralized Timestamps. Proceeding ICDCS '01 Proceedings of the The 21st International Conference on Distributed Computing Systems, Page 585. 
 23. Shapiro, Marc; Preguiça, Nuno; Baquero, Carlos; Zawirski, Marek (13 January 2011). "A Comprehensive Study of Convergent and Commutative Replicated Data Types". _RR-7506_. HAL - Inria.
 24. Vitor Enes, Carlos Baquero, Paulo Sérgio Almeida, Ali Shoker. Join Decompositions for Efficient Synchronization of CRDTs after a Network Partition: Work in progress report. Proceeding [PMLDC '16](http://2016.ecoop.org/track/PMLDC-2016 "Conference Website")  First Workshop on Programming Models and Languages for Distributed Computing, Article No. 6.
 25. On-Line Encyclopedia of Integer Sequences, A151024. https://oeis.org/A151024
 26. Giuseppe DeCandia, Deniz Hastorun, Madan Jampani, Gunavardhan Kakulapati, Avinash Lakshman, Alex Pilchin, Swaminathan Sivasubramanian, Peter Vosshall, and Werner Vogels. Dynamo: Amazon’s highly available key-value store. In Symp. on Op. Sys. Principles (SOSP), volume 41 of Operating Systems Review, pages 205–220, Stevenson, Washington, USA, October 2007. ACM
 27. Cihan Biyikoglu, Under the Hood: Redis CRDTs, WHITE PAPER, RedisLabs.
 28. https://docs.microsoft.com/en-us/azure/cosmos-db/multi-region-writers
 29. https://github.com/automerge/automerge/blob/6bed9b0650e9beacdf4ea2996dcea6fb07af55b5/README.md#caveats