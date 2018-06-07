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

## Poset Model.

## Poset Dimesion. 

## Poset Сanonical Form.

## Extra Definitions.

## Ψ Isomorphism.

## Searching of Ψ of dimension 2.

## Results

## Future Works

## Bibliography
