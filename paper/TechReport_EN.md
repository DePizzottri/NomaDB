# I. In searching of optimal antienthropy algorithm for δ-CmRDT. Work in progress report.
# II. In searching of optimal data causality tracking mechanism. Work in progress report.

### Abstract.
The approach to minimize memory consuption of data causality traching mechanism is proposed. Syncronization chemes that forms partially ordered set of dimention two are explored and the algorithm of checking the poset for being of dimension two is described. There was found and described some general and specific schemes, in wich there is a possibility to make such cauality tracking mechnisms.

## Problem Description.

## CRDT.

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
