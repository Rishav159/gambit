//
// FILE: efg.cc -- Implementation of extensive form data type
//
//  $Id$
//

#include "base/base.h"
#include "math/rational.h"

#ifdef __GNUG__
#pragma implementation "infoset.h"
#pragma implementation "efplayer.h"
#pragma implementation "node.h"
#endif   // __GNUG__

#include "efg.h"
#include "efgutils.h"
#include "efstrat.h"

//----------------------------------------------------------------------
//                 EFPlayer: Member function definitions
//----------------------------------------------------------------------

EFPlayer::~EFPlayer()
{
  while (infosets.Length())   delete infosets.Remove(1);
}


//----------------------------------------------------------------------
//                 Action: Member function definitions
//----------------------------------------------------------------------

bool Action::Precedes(const Node * n) const
{
  while ( n != n->Game()->RootNode() ) {
    if ( n->GetAction() == this )
      return true;
    else
      n = n->GetParent();
  }
  return false;
}

//----------------------------------------------------------------------
//                 Infoset: Member function definitions
//----------------------------------------------------------------------

bool Infoset::Precedes(const Node * n) const
{
  while ( n != n->Game()->RootNode() ) {
    if ( n->GetInfoset() == this )
      return true;
    else
      n = n->GetParent();
  }
  return false;
}

Infoset::Infoset(efgGame *e, int n, EFPlayer *p, int br)
  : E(e), number(n), player(p), actions(br), flag(0) 
{
  while (br)   {
    actions[br] = new Action(br, "", this);
    br--; 
  }
}

Infoset::~Infoset()  
{
  for (int i = 1; i <= actions.Length(); i++)  delete actions[i];
}

void Infoset::PrintActions(gOutput &f) const
{ 
  f << "{ ";
  for (int i = 1; i <= actions.Length(); i++)
    f << '"' << EscapeQuotes(actions[i]->name) << "\" ";
  f << "}";
}

Action *Infoset::InsertAction(int where)
{
  Action *action = new Action(where, "", this);
  actions.Insert(action, where);
  for (; where <= actions.Length(); where++)
    actions[where]->number = where;
  return action;
}

void Infoset::RemoveAction(int which)
{
  delete actions.Remove(which);
  for (; which <= actions.Length(); which++)
    actions[which]->number = which;
}

const gList<Action *> Infoset::ListOfActions(void) const
{
  gList<Action *> answer;
  for (int i = 1; i <= actions.Length(); i++) 
    answer += actions[i];
  return answer;
}

const gList<const Node *> Infoset::ListOfMembers(void) const
{
  gList<const Node *> answer;
  for (int i = 1; i <= members.Length(); i++) 
    answer += members[i];
  return answer;
}

//------------------------------------------------------------------------
//           ChanceInfoset: Member function definitions
//------------------------------------------------------------------------

ChanceInfoset::ChanceInfoset(efgGame *E, int n, EFPlayer *p, int br)
  : Infoset(E, n, p, br), probs(br)
{
  for (int i = 1; i <= br; probs[i++] = gRational(1, br));
}

Action *ChanceInfoset::InsertAction(int where)
{ 
  Action *action = Infoset::InsertAction(where);
  probs.Insert((gNumber) 0, where);
  return action;
}

void ChanceInfoset::RemoveAction(int which)
{
  Infoset::RemoveAction(which);
  probs.Remove(which);
}

void ChanceInfoset::PrintActions(gOutput &f) const
{ 
  f << "{ ";
  for (int i = 1; i <= actions.Length(); i++) {
    f << '"' << EscapeQuotes(actions[i]->GetName()) << "\" ";
    f << probs[i] << ' ';
  }
  f << "}";
}

//----------------------------------------------------------------------
//                   Node: Member function definitions
//----------------------------------------------------------------------

Node::Node(efgGame *e, Node *p)
  : mark(false), number(0), E(e), infoset(0), parent(p), outcome(0),
    gameroot((p) ? p->gameroot : this)
{ }

Node::~Node()
{
  for (int i = children.Length(); i; delete children[i--]);
}


int Node::NumberInInfoset(void) const
{
  for (int i = 1; i <= GetInfoset()->NumMembers(); i++)
    if (GetInfoset()->GetMember(i) == this)
      return i;
  //  This could be speeded up by adding a member to Node to keep track of this
  throw efgGame::Exception();
}


Node *Node::NextSibling(void) const  
{
  if (!parent)   return 0;
  if (parent->children.Find((Node * const) this) == parent->children.Length())
    return 0;
  else
    return parent->children[parent->children.Find((Node * const)this) + 1];
}

Node *Node::PriorSibling(void) const
{ 
  if (!parent)   return 0;
  if (parent->children.Find((Node * const)this) == 1)
    return 0;
  else
    return parent->children[parent->children.Find((Node * const)this) - 1];

}

Action *Node::GetAction(void) const
{
  if (this == Game()->RootNode()) {
    return 0;
  }
  
  const gArray<Action *> &actions = GetParent()->GetInfoset()->Actions();
  for (int i = 1; i <= actions.Length(); i++) {
    if (this == GetParent()->GetChild(actions[i])) {
      return actions[i];
    }
  }

  return 0;
}

void Node::DeleteOutcome(efgOutcome *outc)
{
  if (outc == outcome)   outcome = 0;
  for (int i = 1; i <= children.Length(); i++)
    children[i]->DeleteOutcome(outc);
}


//------------------------------------------------------------------------
//       Efg: Constructors, destructor, constructive operators
//------------------------------------------------------------------------

#ifdef MEMCHECK
int efgGame::_NumObj = 0;
#endif // MEMCHECK

efgGame::efgGame(void)
  : sortisets(true), m_dirty(false), m_revision(0), 
    m_outcome_revision(-1), title("UNTITLED"),
    chance(new EFPlayer(this, 0)), afg(0), lexicon(0)
{
  root = new Node(this, 0);
#ifdef MEMCHECK
  _NumObj++;
  gout << "--- Efg Ctor: " << _NumObj << "\n";
#endif // MEMCHECK

  SortInfosets();
}

efgGame::efgGame(const efgGame &E, Node *n /* = 0 */)
  : sortisets(false), m_dirty(false), m_revision(0), 
    m_outcome_revision(-1), title(E.title), comment(E.comment),
    players(E.players.Length()), outcomes(0, E.outcomes.Last()),
    chance(new EFPlayer(this, 0)),
    afg(0), lexicon(0) 
{
  for (int i = 1; i <= players.Length(); i++)  {
    (players[i] = new EFPlayer(this, i))->name = E.players[i]->name;
    for (int j = 1; j <= E.players[i]->infosets.Length(); j++)   {
      Infoset *s = new Infoset(this, j, players[i],
			       E.players[i]->infosets[j]->actions.Length());
      s->name = E.players[i]->infosets[j]->name;
      for (int k = 1; k <= s->actions.Length(); k++)
	s->actions[k]->name = E.players[i]->infosets[j]->actions[k]->name;
      players[i]->infosets.Append(s);
    }
  }

  for (int i = 1; i <= E.GetChance()->NumInfosets(); i++)   {
    ChanceInfoset *t = (ChanceInfoset *) E.GetChance()->Infosets()[i];
    ChanceInfoset *s = new ChanceInfoset(this, i, chance,
					       t->NumActions());
    s->name = t->GetName();
    for (int act = 1; act <= s->probs.Length(); act++) {
      s->probs[act] = t->probs[act];
      s->actions[act]->name = t->actions[act]->name;
    }
    chance->infosets.Append(s);
  }

  for (int outc = 1; outc <= E.NumOutcomes(); outc++)  {
    outcomes[outc] = new efgOutcome(this, outc);
    outcomes[outc]->m_name = E.outcomes[outc]->m_name;
    outcomes[outc]->m_payoffs = E.outcomes[outc]->m_payoffs;
  }

  root = new Node(this, 0);
  CopySubtree(root, (n ? n : E.RootNode()));
  
  if (n)   {
    for (int pl = 1; pl <= players.Length(); pl++)  {
      for (int i = 1; i <= players[pl]->infosets.Length(); i++)  {
	if (players[pl]->infosets[i]->members.Length() == 0)
	  delete players[pl]->infosets.Remove(i--);
      }
    }
  }

  sortisets = true;
  SortInfosets();
}

#include "lexicon.h"

efgGame::~efgGame()
{
  delete root;
  delete chance;

  for (int i = 1; i <= players.Length(); delete players[i++]);
  for (int i = 1; i <= outcomes.Last(); delete outcomes[i++]);

  if (lexicon)   delete lexicon;
  lexicon = 0;
}

//------------------------------------------------------------------------
//                  Efg: Private member functions
//------------------------------------------------------------------------

void efgGame::DeleteLexicon(void) const
{
  if (lexicon)   delete lexicon;
  lexicon = 0;
}

Infoset *efgGame::GetInfosetByIndex(EFPlayer *p, int index) const
{
  for (int i = 1; i <= p->infosets.Length(); i++)
    if (p->infosets[i]->number == index)   return p->infosets[i];
  return 0;
}

efgOutcome *efgGame::GetOutcomeByIndex(int index) const
{
  for (int i = 1; i <= outcomes.Last(); i++) {
    if (outcomes[i]->m_number == index)  {
      return outcomes[i];
    }
  }

  return 0;
}

void efgGame::Reindex(void)
{
  int i;

  for (i = 1; i <= players.Length(); i++)  {
    EFPlayer *p = players[i];
    for (int j = 1; j <= p->infosets.Length(); j++)
      p->infosets[j]->number = j;
  }

  for (i = 1; i <= outcomes.Last(); i++)
    outcomes[i]->m_number = i;
}


void efgGame::NumberNodes(Node *n, int &index)
{
  n->number = index++;
  for (int child = 1; child <= n->children.Length();
       NumberNodes(n->children[child++], index));
} 

void efgGame::SortInfosets(void)
{
  if (!sortisets)  return;

  int pl;

  for (pl = 0; pl <= players.Length(); pl++)  {
    gList<Node *> nodes;

    Nodes(*this, nodes);

    EFPlayer *player = (pl) ? players[pl] : chance;

    int i, isets = 0;

    // First, move all empty infosets to the back of the list so
    // we don't "lose" them
    int foo = player->infosets.Length();
    i = 1;
    while (i < foo)   {
      if (player->infosets[i]->members.Length() == 0)  {
	Infoset *bar = player->infosets[i];
	player->infosets[i] = player->infosets[foo];
	player->infosets[foo--] = bar;
      }
      else
	i++;
    }

    // This will give empty infosets their proper number; the nonempty
    // ones will be renumbered by the next loop
    for (i = 1; i <= player->infosets.Length(); i++)
      if (player->infosets[i]->members.Length() == 0)
	player->infosets[i]->number = i;
      else
	player->infosets[i]->number = 0;
  
    for (i = 1; i <= nodes.Length(); i++)  {
      Node *n = nodes[i];
      if (n->GetPlayer() == player && n->GetInfoset()->number == 0)  {
	n->GetInfoset()->number = ++isets;
	player->infosets[isets] = n->GetInfoset();
      }
    }  
  }

  // Now, we sort the nodes within the infosets
  
  gList<Node *> nodes;
  Nodes(*this, nodes);

  for (pl = 0; pl <= players.Length(); pl++)  {
    EFPlayer *player = (pl) ? players[pl] : chance;

    for (int iset = 1; iset <= player->infosets.Length(); iset++)  {
      Infoset *s = player->infosets[iset];
      for (int i = 1, j = 1; i <= nodes.Length(); i++)  {
	if (nodes[i]->infoset == s)
	  s->members[j++] = nodes[i];
      }
    }
  }

  int nodeindex = 1;
  NumberNodes(root, nodeindex);
}
  
Infoset *efgGame::CreateInfoset(int n, EFPlayer *p, int br)
{
  Infoset *s = (p->IsChance()) ? new ChanceInfoset(this, n, p, br) :
               new Infoset(this, n, p, br);
  p->infosets.Append(s);
  return s;
}

efgOutcome *efgGame::CreateOutcomeByIndex(int index)
{
  NewOutcome(index);
  return outcomes[outcomes.Last()];
}

void efgGame::CopySubtree(Node *n, Node *m)
{
  n->name = m->name;

  if (m->gameroot == m)
    n->gameroot = n;

  if (m->outcome) {
    n->outcome = m->outcome;
  }

  if (m->infoset)   {
    EFPlayer *p;
    if (m->infoset->player->number)
      p = players[m->infoset->player->number];
    else
      p = chance;

    Infoset *s = p->Infosets()[m->infoset->number];
    AppendNode(n, s);

    for (int i = 1; i <= n->children.Length(); i++)
      CopySubtree(n->children[i], m->children[i]);
  }
}

//------------------------------------------------------------------------
//               Efg: Title access and manipulation
//------------------------------------------------------------------------

void efgGame::SetTitle(const gText &s)
{
  title = s; 
  m_revision++;
  m_dirty = true;
}

const gText &efgGame::GetTitle(void) const
{ return title; }

void efgGame::SetComment(const gText &s)
{
  comment = s;
  m_revision++;
  m_dirty = true;
}

const gText &efgGame::GetComment(void) const
{ return comment; }
  

//------------------------------------------------------------------------
//                    Efg: Writing data files
//------------------------------------------------------------------------

void efgGame::WriteEfgFile(gOutput &f, Node *n) const
{
  if (n->children.Length() == 0)   {
    f << "t \"" << EscapeQuotes(n->name) << "\" ";
    if (n->outcome)  {
      f << n->outcome->m_number << " \"" <<
	EscapeQuotes(n->outcome->m_name) << "\" ";
      f << "{ ";
      for (int pl = 1; pl <= NumPlayers(); pl++)  {
	f << n->outcome->m_payoffs[pl];
	if (pl < NumPlayers())
	  f << ", ";
	else
	  f << " }\n";
      }
    }
    else
      f << "0\n";
  }

  else if (n->infoset->player->number)   {
    f << "p \"" << EscapeQuotes(n->name) << "\" " <<
      n->infoset->player->number << ' ';
    f << n->infoset->number << " \"" <<
      EscapeQuotes(n->infoset->name) << "\" ";
    n->infoset->PrintActions(f);
    f << " ";
    if (n->outcome)  {
      f << n->outcome->m_number << " \"" <<
	EscapeQuotes(n->outcome->m_name) << "\" ";
      f << "{ ";
      for (int pl = 1; pl <= NumPlayers(); pl++)  {
	f << n->outcome->m_payoffs[pl];
	if (pl < NumPlayers())
	  f << ", ";
	else
	  f << " }\n";
      }
    }
    else
      f << "0\n";
  }

  else   {    // chance node
    f << "c \"" << n->name << "\" ";
    f << n->infoset->number << " \"" <<
      EscapeQuotes(n->infoset->name) << "\" ";
    n->infoset->PrintActions(f);
    f << " ";
    if (n->outcome)  {
      f << n->outcome->m_number << " \"" <<
	EscapeQuotes(n->outcome->m_name) << "\" ";
      f << "{ ";
      for (int pl = 1; pl <= NumPlayers(); pl++)  {
	f << n->outcome->m_payoffs[pl];
        if (pl < NumPlayers()) 
          f << ", ";
        else
          f << " }\n";
      }
    }
    else
      f << "0\n";
  }

  for (int i = 1; i <= n->children.Length(); i++)
    WriteEfgFile(f, n->children[i]);
}

void efgGame::WriteEfgFile(gOutput &p_file, int p_nDecimals) const
{
  int oldPrecision = p_file.GetPrec();
  p_file.SetPrec(p_nDecimals);

  try {
    p_file << "EFG 2 R";
    p_file << " \"" << EscapeQuotes(title) << "\" { ";
    for (int i = 1; i <= players.Length(); i++)
      p_file << '"' << EscapeQuotes(players[i]->name) << "\" ";
    p_file << "}\n";
    p_file << "\"" << EscapeQuotes(comment) << "\"\n\n";

    WriteEfgFile(p_file, root);
    p_file.SetPrec(oldPrecision);
    m_revision++;
    m_dirty = false;
  }
  catch (...) {
    p_file.SetPrec(oldPrecision);
    throw;
  }
}


//------------------------------------------------------------------------
//                    Efg: General data access
//------------------------------------------------------------------------

int efgGame::NumPlayers(void) const
{ return players.Length(); }

EFPlayer *efgGame::NewPlayer(void)
{
  m_revision++;
  m_dirty = true;

  EFPlayer *ret = new EFPlayer(this, players.Length() + 1);
  players.Append(ret);

  for (int outc = 1; outc <= outcomes.Last();
       outcomes[outc++]->m_payoffs.Append(0));
  for (int outc = 1; outc <= outcomes.Last();
       outcomes[outc++]->m_doublePayoffs.Append(0));
  DeleteLexicon();
  return ret;
}

gBlock<Infoset *> efgGame::Infosets() const
{
  gBlock<Infoset *> answer;

  gArray<EFPlayer *> p = Players();
  int i;
  for (i = 1; i <= p.Length(); i++) {
    gArray<Infoset *> infosets_for_player = p[i]->Infosets();
    int j;
    for (j = 1; j <= infosets_for_player.Length(); j++)
      answer += infosets_for_player[j];
  }

  return answer;
}

int efgGame::NumOutcomes(void) const
{ return outcomes.Last(); }

efgOutcome *efgGame::NewOutcome(void)
{
  m_revision++;
  m_dirty = true;
  return NewOutcome(outcomes.Last() + 1);
}

void efgGame::DeleteOutcome(efgOutcome *p_outcome)
{
  m_revision++;
  m_dirty = true;

  root->DeleteOutcome(p_outcome);
  delete outcomes.Remove(outcomes.Find(p_outcome));
  DeleteLexicon();
}

efgOutcome *efgGame::GetOutcome(int p_index) const
{
  return outcomes[p_index];
}

void efgGame::SetOutcomeName(efgOutcome *p_outcome, const gText &p_name)
{
  p_outcome->m_name = p_name;
}

const gText &efgGame::GetOutcomeName(efgOutcome *p_outcome) const
{
  return p_outcome->m_name;
}

efgOutcome *efgGame::GetOutcome(const Node *p_node) const
{
  return p_node->outcome;
}

void efgGame::SetOutcome(Node *p_node, efgOutcome *p_outcome)
{
  p_node->outcome = p_outcome;
}

void efgGame::SetPayoff(efgOutcome *p_outcome, int pl, 
			const gNumber &value)
{
  if (!p_outcome) {
    return;
  }

  m_revision++;
  m_dirty = true;
  p_outcome->m_payoffs[pl] = value;
  p_outcome->m_doublePayoffs[pl] = (double) value;
}

gNumber efgGame::Payoff(efgOutcome *p_outcome,
			const EFPlayer *p_player) const
{
  if (!p_outcome) {
    return gNumber(0);
  }

  return p_outcome->m_payoffs[p_player->number];
}

gNumber efgGame::Payoff(const Node *p_node, const EFPlayer *p_player) const
{
  return (p_node->outcome) ? p_node->outcome->m_payoffs[p_player->number] : gNumber(0);
}

gArray<gNumber> efgGame::Payoff(efgOutcome *p_outcome) const
{
  if (!p_outcome) {
    gArray<gNumber> ret(players.Length());
    for (int i = 1; i <= ret.Length(); ret[i++] = 0);
    return ret;
  }
  else {
    return p_outcome->m_payoffs;
  }
}

bool efgGame::IsConstSum(void) const
{
  int pl, index;
  gNumber cvalue = (gNumber) 0;

  if (outcomes.Last() == 0)  return true;

  for (pl = 1; pl <= players.Length(); pl++)
    cvalue += outcomes[1]->m_payoffs[pl];

  for (index = 2; index <= outcomes.Last(); index++)  {
    gNumber thisvalue(0);

    for (pl = 1; pl <= players.Length(); pl++)
      thisvalue += outcomes[index]->m_payoffs[pl];

    if (thisvalue > cvalue || thisvalue < cvalue)
      return false;
  }

  return true;
}

gNumber efgGame::MinPayoff(int pl) const
{
  int index, p, p1, p2;
  gNumber minpay;

  if (NumOutcomes() == 0)  return 0;

  if(pl) { p1=p2=pl;}
  else {p1=1;p2=players.Length();}

  minpay = outcomes[1]->m_payoffs[p1];

  for (index = 1; index <= outcomes.Last(); index++)  {
    for (p = p1; p <= p2; p++)
      if (outcomes[index]->m_payoffs[p] < minpay)
	minpay = outcomes[index]->m_payoffs[p];
  }
  return minpay;
}

gNumber efgGame::MaxPayoff(int pl) const
{
  int index, p, p1, p2;
  gNumber maxpay;

  if (NumOutcomes() == 0)  return 0;

  if(pl) { p1=p2=pl;}
  else {p1=1;p2=players.Length();}

  maxpay = outcomes[1]->m_payoffs[p1];

  for (index = 1; index <= outcomes.Last(); index++)  {
    for (p = p1; p <= p2; p++)
      if (outcomes[index]->m_payoffs[p] > maxpay)
	maxpay = outcomes[index]->m_payoffs[p];
  }
  return maxpay;
}

Node *efgGame::RootNode(void) const
{ return root; }

bool efgGame::IsSuccessor(const Node *n, const Node *from) const
{ return IsPredecessor(from, n); }

bool efgGame::IsPredecessor(const Node *n, const Node *of) const
{
  while (of && n != of)    of = of->parent;

  return (n == of);
}

gArray<int> efgGame::PathToNode(const Node *p_node) const
{
  gBlock<int> ret;
  const Node *n = p_node;
  while (n->GetParent()) {
    ret.Insert(p_node->GetAction()->GetNumber(), 1);
    n = n->GetParent();
  }
  return ret;
}

void efgGame::DescendantNodes(const Node* n, 
			      const EFSupport& supp,
			      gList<Node *> &current) const
{
  current += const_cast<Node *>(n);
  if (n->IsNonterminal()) {
    const gArray<Action *> actions = supp.Actions(n->GetInfoset());
    for (int i = 1; i <= actions.Length(); i++) {
      const Node* newn = n->GetChild(actions[i]);
      DescendantNodes(newn,supp,current);
    }
  }
}

void efgGame::NonterminalDescendants(const Node* n, 
					  const EFSupport& supp,
					  gList<const Node*>& current) const
{
  if (n->IsNonterminal()) {
    current += n;
    const gArray<Action *> actions = supp.Actions(n->GetInfoset());
    for (int i = 1; i <= actions.Length(); i++) {
      const Node* newn = n->GetChild(actions[i]);
      NonterminalDescendants(newn,supp,current);
    }
  }
}

void efgGame::TerminalDescendants(const Node* n, 
				  const EFSupport& supp,
				  gList<Node *> &current) const
{
  if (n->IsTerminal()) { 
    // casting away const to silence compiler warning
    current += (Node *) n;
  }
  else {
    const gArray<Action *> actions = supp.Actions(n->GetInfoset());
    for (int i = 1; i <= actions.Length(); i++) {
      const Node* newn = n->GetChild(actions[i]);
      TerminalDescendants(newn,supp,current);
    }
  }
}

gList<Node *>
efgGame::DescendantNodes(const Node &p_node, const EFSupport &p_support) const
{
  gList<Node *> answer;
  DescendantNodes(&p_node, p_support, answer);
  return answer;
}

gList<const Node*> 
efgGame::NonterminalDescendants(const Node& n, const EFSupport& supp) const
{
  gList<const Node*> answer;
  NonterminalDescendants(&n,supp,answer);
  return answer;
}

gList<Node *> 
efgGame::TerminalDescendants(const Node& n, const EFSupport& supp) const
{
  gList<Node *> answer;
  TerminalDescendants(&n,supp,answer);
  return answer;
}

gList<Node *> efgGame::TerminalNodes() const
{
  return TerminalDescendants(*(RootNode()),EFSupport(*this));
}

gList<Infoset*> efgGame::DescendantInfosets(const Node& n, 
					const EFSupport& supp) const
{
  gList<Infoset*> answer;
  gList<const Node*> nodelist = NonterminalDescendants(n,supp);
  int i;
  for (i = 1; i <= nodelist.Length(); i++) {
      Infoset* iset = nodelist[i]->GetInfoset();
      if (!answer.Contains(iset))
	answer += iset;
  }
  return answer;
}

const gArray<Node *> &efgGame::Children(const Node *n) const
{ return n->children; }

int efgGame::NumChildren(const Node *n) const
{ return n->children.Length(); }

efgOutcome *efgGame::NewOutcome(int index)
{
  m_revision++;
  m_dirty = true;
  outcomes.Append(new efgOutcome(this, index));
  return outcomes[outcomes.Last()];
} 

//------------------------------------------------------------------------
//                     Efg: Operations on players
//------------------------------------------------------------------------

EFPlayer *efgGame::GetChance(void) const
{
  return chance;
}

Infoset *efgGame::AppendNode(Node *n, EFPlayer *p, int count)
{
  if (!n || !p || count == 0)
    throw Exception();

  m_revision++;
  m_dirty = true;

  if (n->children.Length() == 0)   {
    n->infoset = CreateInfoset(p->infosets.Length() + 1, p, count);
    n->infoset->members.Append(n);
    while (count--)
      n->children.Append(new Node(this, n));
  }

  DeleteLexicon();
  SortInfosets();
  return n->infoset;
}  

Infoset *efgGame::AppendNode(Node *n, Infoset *s)
{
  if (!n || !s)   throw Exception();
  
  // Can't bridge subgames...
  if (s->members.Length() > 0 && n->gameroot != s->members[1]->gameroot)
    return 0;

  if (n->children.Length() == 0)   {
    m_revision++;
    m_dirty = true;
    n->infoset = s;
    s->members.Append(n);
    for (int i = 1; i <= s->actions.Length(); i++)
      n->children.Append(new Node(this, n));
  }

  DeleteLexicon();
  SortInfosets();
  return s;
}
  
Node *efgGame::DeleteNode(Node *n, Node *keep)
{
  if (!n || !keep)   throw Exception();

  if (keep->parent != n)   return n;

  if (n->gameroot == n)
    MarkSubgame(keep, keep);

  m_revision++;
  m_dirty = true;
  // turn infoset sorting off during tree deletion -- problems will occur
  sortisets = false;

  n->children.Remove(n->children.Find(keep));
  DeleteTree(n);
  keep->parent = n->parent;
  if (n->parent)
    n->parent->children[n->parent->children.Find(n)] = keep;
  else
    root = keep;

  delete n;
  DeleteLexicon();

  sortisets = true;

  SortInfosets();
  return keep;
}

Infoset *efgGame::InsertNode(Node *n, EFPlayer *p, int count)
{
  if (!n || !p || count <= 0)  throw Exception();

  m_revision++;
  m_dirty = true;

  Node *m = new Node(this, n->parent);
  m->infoset = CreateInfoset(p->infosets.Length() + 1, p, count);
  m->infoset->members.Append(m);
  if (n->parent)
    n->parent->children[n->parent->children.Find(n)] = m;
  else
    root = m;
  m->children.Append(n);
  n->parent = m;
  while (--count)
    m->children.Append(new Node(this, m));

  DeleteLexicon();
  SortInfosets();
  return m->infoset;
}

Infoset *efgGame::InsertNode(Node *n, Infoset *s)
{
  if (!n || !s)  throw Exception();

  // can't bridge subgames
  if (s->members.Length() > 0 && n->gameroot != s->members[1]->gameroot)
    return 0;
  
  m_revision++;
  m_dirty = true;

  Node *m = new Node(this, n->parent);
  m->infoset = s;
  s->members.Append(m);
  if (n->parent)
    n->parent->children[n->parent->children.Find(n)] = m;
  else
    root = m;
  m->children.Append(n);
  n->parent = m;
  int count = s->actions.Length();
  while (--count)
    m->children.Append(new Node(this, m));

  DeleteLexicon();
  SortInfosets();
  return m->infoset;
}

Infoset *efgGame::CreateInfoset(EFPlayer *p, int br)
{
  if (!p || p->Game() != this)  throw Exception();
  m_revision++;
  m_dirty = true;
  return CreateInfoset(p->infosets.Length() + 1, p, br);
}

Infoset *efgGame::JoinInfoset(Infoset *s, Node *n)
{
  if (!n || !s)  throw Exception();

  // can't bridge subgames
  if (s->members.Length() > 0 && n->gameroot != s->members[1]->gameroot)
    return 0;
  
  if (!n->infoset)   return 0; 
  if (n->infoset == s)   return s;
  if (s->actions.Length() != n->children.Length())  return n->infoset;

  m_revision++;
  m_dirty = true;

  Infoset *t = n->infoset;

  t->members.Remove(t->members.Find(n));
  s->members.Append(n);

  n->infoset = s;

  DeleteLexicon();
  SortInfosets();
  return s;
}

Infoset *efgGame::LeaveInfoset(Node *n)
{
  if (!n)  throw Exception();

  if (!n->infoset)   return 0;

  Infoset *s = n->infoset;
  if (s->members.Length() == 1)   return s;

  m_revision++;
  m_dirty = true;

  EFPlayer *p = s->player;
  s->members.Remove(s->members.Find(n));
  n->infoset = CreateInfoset(p->infosets.Length() + 1, p,
			     n->children.Length());
  n->infoset->name = s->name;
  n->infoset->members.Append(n);
  for (int i = 1; i <= s->actions.Length(); i++)
    n->infoset->actions[i]->name = s->actions[i]->name;

  DeleteLexicon();
  SortInfosets();
  return n->infoset;
}

Infoset *efgGame::SplitInfoset(Node *n)
{
  if (!n)  throw Exception();

  if (!n->infoset)   return 0;

  Infoset *s = n->infoset;
  if (s->members.Length() == 1)   return s;

  m_revision++;
  m_dirty = true;

  EFPlayer *p = s->player;
  Infoset *ns = CreateInfoset(p->infosets.Length() + 1, p,
			      n->children.Length());
  ns->name = s->name;
  int i;
  for (i = s->members.Length(); i > s->members.Find(n); i--)   {
    Node *nn = s->members.Remove(i);
    ns->members.Append(nn);
    nn->infoset = ns;
  }
  for (i = 1; i <= s->actions.Length(); i++) {
    ns->actions[i]->name = s->actions[i]->name;
    if (p == chance) {
      SetChanceProb(ns, i, GetChanceProb(s, i));
    }
  }
  DeleteLexicon();
  SortInfosets();
  return n->infoset;
}

Infoset *efgGame::MergeInfoset(Infoset *to, Infoset *from)
{
  if (!to || !from)  throw Exception();

  if (to == from ||
      to->actions.Length() != from->actions.Length())   return from;

  if (to->members[1]->gameroot != from->members[1]->gameroot) 
    return from;

  m_revision++;
  m_dirty = true;

  to->members += from->members;
  for (int i = 1; i <= from->members.Length(); i++)
    from->members[i]->infoset = to;

  from->members.Flush();

  DeleteLexicon();
  SortInfosets();
  return to;
}

bool efgGame::DeleteEmptyInfoset(Infoset *s)
{
  if (!s)  throw Exception();

  if (s->NumMembers() > 0)   return false;

  m_revision++;
  m_dirty = true;
  s->player->infosets.Remove(s->player->infosets.Find(s));
  delete s;

  return true;
}

void efgGame::DeleteEmptyInfosets(void)
{
  for (int pl = 1; pl <= NumPlayers(); pl++) {
    for (int iset = 1; iset <= NumInfosets()[pl]; iset++) {
      if (DeleteEmptyInfoset(Players()[pl]->Infosets()[iset])) {
        iset--;
      }
    }
  }
} 

Infoset *efgGame::SwitchPlayer(Infoset *s, EFPlayer *p)
{
  if (!s || !p)  throw Exception();
  if (s->GetPlayer()->IsChance() || p->IsChance())  throw Exception();
  
  if (s->player == p)   return s;

  m_revision++;
  m_dirty = true;
  s->player->infosets.Remove(s->player->infosets.Find(s));
  s->player = p;
  p->infosets.Append(s);

  DeleteLexicon();
  SortInfosets();
  return s;
}

void efgGame::CopySubtree(Node *src, Node *dest, Node *stop)
{
  if (src == stop) {
    dest->outcome = src->outcome;
    return;
  }

  if (src->children.Length())  {
    AppendNode(dest, src->infoset);
    for (int i = 1; i <= src->children.Length(); i++)
      CopySubtree(src->children[i], dest->children[i], stop);
  }

  dest->name = src->name;
  dest->outcome = src->outcome;
}

//
// MarkSubtree: sets the Node::mark flag on all children of p_node
//
void efgGame::MarkSubtree(Node *p_node)
{
  p_node->mark = true;
  for (int i = 1; i <= p_node->children.Length(); i++) {
    MarkSubtree(p_node->children[i]);
  }
}

//
// UnmarkSubtree: clears the Node::mark flag on all children of p_node
//
void efgGame::UnmarkSubtree(Node *p_node)
{
  p_node->mark = false;
  for (int i = 1; i <= p_node->children.Length(); i++) {
    UnmarkSubtree(p_node->children[i]);
  }
}

void efgGame::Reveal(Infoset *where, const gArray<EFPlayer *> &who)
{
  if (where->actions.Length() <= 1)  {
    // only one action; nothing to reveal!
    return;
  }

  UnmarkSubtree(root);  // start with a clean tree
  
  m_revision++;
  m_dirty = true;

  for (int i = 1; i <= where->actions.Length(); i++) {
    for (int j = 1; j <= where->members.Length(); j++) { 
      MarkSubtree(where->members[j]->children[i]);
    }

    for (int j = who.First(); j <= who.Last(); j++) {
      // iterate over each information set of player 'j' in the list
      for (int k = 1; k <= who[j]->infosets.Length(); k++) {
	// iterate over each member of information set 'k'
	// make copy of members to iterate correctly 
	// (since the information set may be changed in the process)
	gArray<Node *> members = who[j]->infosets[k]->members;
	Infoset *newiset = 0;

	for (int m = 1; m <= members.Length(); m++) {
	  Node *n = members[m];
	  if (n->mark) {
	    // If node is marked, is descendant of action 'i'
	    n->mark = false;   // unmark so tree is clean at end
	    if (!newiset) {
	      newiset = LeaveInfoset(n);
	    }
	    else {
	      JoinInfoset(newiset, n);
	    }
	  } 
	}
      }
    }
  }

  Reindex();
}

Node *efgGame::CopyTree(Node *src, Node *dest)
{
  if (!src || !dest)  throw Exception();
  if (src == dest || dest->children.Length())   return src;
  if (src->gameroot != dest->gameroot)  return src;

  if (src->children.Length())  {
    m_revision++;
    m_dirty = true;

    AppendNode(dest, src->infoset);
    for (int i = 1; i <= src->children.Length(); i++)
      CopySubtree(src->children[i], dest->children[i], dest);

    DeleteLexicon();
    SortInfosets();
  }

  return dest;
}

Node *efgGame::MoveTree(Node *src, Node *dest)
{
  if (!src || !dest)  throw Exception();
  if (src == dest || dest->children.Length() || IsPredecessor(src, dest))
    return src;
  if (src->gameroot != dest->gameroot)  return src;

  m_revision++;
  m_dirty = true;

  if (src->parent == dest->parent) {
    int srcChild = src->parent->children.Find(src);
    int destChild = src->parent->children.Find(dest);
    src->parent->children[srcChild] = dest;
    src->parent->children[destChild] = src;
  }
  else {
    Node *parent = src->parent; 
    parent->children[parent->children.Find(src)] = dest;
    dest->parent->children[dest->parent->children.Find(dest)] = src;
    src->parent = dest->parent;
    dest->parent = parent;
  }

  dest->name = "";
  dest->outcome = 0;
  
  DeleteLexicon();
  SortInfosets();
  return dest;
}

Node *efgGame::DeleteTree(Node *n)
{
  if (!n)  throw Exception();

  m_revision++;
  m_dirty = true;

  while (NumChildren(n) > 0)   {
    DeleteTree(n->children[1]);
    delete n->children.Remove(1);
  }
  
  if (n->infoset)  {
    n->infoset->members.Remove(n->infoset->members.Find(n));
    n->infoset = 0;
  }
  n->outcome = 0;
  n->name = "";

  DeleteLexicon();
  SortInfosets();
  return n;
}

Action *efgGame::InsertAction(Infoset *s)
{
  if (!s)  throw Exception();

  m_revision++;
  m_dirty = true;
  Action *action = s->InsertAction(s->NumActions() + 1);
  for (int i = 1; i <= s->members.Length(); i++) {
    s->members[i]->children.Append(new Node(this, s->members[i]));
  }
  DeleteLexicon();
  SortInfosets();
  return action;
}

Action *efgGame::InsertAction(Infoset *s, const Action *a)
{
  if (!a || !s)  throw Exception();

  m_revision++;
  m_dirty = true;

  int where;
  for (where = 1; where <= s->actions.Length() && s->actions[where] != a;
       where++);
  if (where > s->actions.Length())   return 0;
  Action *action = s->InsertAction(where);
  for (int i = 1; i <= s->members.Length(); i++)
    s->members[i]->children.Insert(new Node(this, s->members[i]), where);

  DeleteLexicon();
  SortInfosets();
  return action;
}

Infoset *efgGame::DeleteAction(Infoset *s, const Action *a)
{
  if (!a || !s)  throw Exception();

  m_revision++;
  m_dirty = true;
  int where;
  for (where = 1; where <= s->actions.Length() && s->actions[where] != a;
       where++);
  if (where > s->actions.Length() || s->actions.Length() == 1)   return s;
  s->RemoveAction(where);
  for (int i = 1; i <= s->members.Length(); i++)   {
    DeleteTree(s->members[i]->children[where]);
    delete s->members[i]->children.Remove(where);
  }
  DeleteLexicon();
  SortInfosets();
  return s;
}

void efgGame::SetChanceProb(Infoset *infoset, int act, const gNumber &value)
{
  if (infoset->IsChanceInfoset()) {
    m_revision++;
    m_dirty = true;
    ((ChanceInfoset *) infoset)->SetActionProb(act, value);
  }
}

gNumber efgGame::GetChanceProb(Infoset *infoset, int act) const
{
  if (infoset->IsChanceInfoset())
    return ((ChanceInfoset *) infoset)->GetActionProb(act);
  else
    return (gNumber) 0;
}

gNumber efgGame::GetChanceProb(const Action *a) const
{
  return GetChanceProbs(a->BelongsTo())[a->GetNumber()];
}

gArray<gNumber> efgGame::GetChanceProbs(Infoset *infoset) const
{
  if (infoset->IsChanceInfoset())
    return ((ChanceInfoset *) infoset)->GetActionProbs();
  else
    return gArray<gNumber>(infoset->NumActions());
}

//---------------------------------------------------------------------
//                     Subgame-related functions
//---------------------------------------------------------------------

void efgGame::MarkTree(Node *n, Node *base)
{
  n->ptr = base;
  for (int i = 1; i <= NumChildren(n); i++)
    MarkTree(n->GetChild(i), base);
}

bool efgGame::CheckTree(Node *n, Node *base)
{
  int i;

  if (NumChildren(n) == 0)   return true;

  for (i = 1; i <= NumChildren(n); i++)
    if (!CheckTree(n->GetChild(i), base))  return false;

  if (n->GetPlayer()->IsChance())   return true;

  for (i = 1; i <= n->GetInfoset()->NumMembers(); i++)
    if (n->GetInfoset()->Members()[i]->ptr != base)
      return false;

  return true;
}

bool efgGame::IsLegalSubgame(Node *n)
{
  if (NumChildren(n) == 0)  
    return false;

  MarkTree(n, n);
  return CheckTree(n, n);
}

bool efgGame::MarkSubgame(Node *n)
{
  if(n->gameroot == n) return true;

  if (n->gameroot != n && IsLegalSubgame(n))  {
    n->gameroot = 0;
    MarkSubgame(n, n);
    return true;
  }

  return false;
}

void efgGame::UnmarkSubgame(Node *n)
{
  if (n->gameroot == n && n->parent)  {
    n->gameroot = 0;
    MarkSubgame(n, n->parent->gameroot);
  }
}
  

void efgGame::MarkSubgame(Node *n, Node *base)
{
  if (n->gameroot == n)  return;
  n->gameroot = base;
  for (int i = 1; i <= NumChildren(n); i++)
    MarkSubgame(n->GetChild(i), base);
}

void efgGame::MarkSubgames(const gList<Node *> &list)
{
  for (int i = 1; i <= list.Length(); i++)  {
    if (IsLegalSubgame(list[i]))  {
      list[i]->gameroot = 0;
      MarkSubgame(list[i], list[i]);
    }
  }
}

void efgGame::MarkSubgames(void)
{
  gList<Node *> subgames;
  LegalSubgameRoots(*this, subgames);

  for (int i = 1; i <= subgames.Length(); i++)  {
    subgames[i]->gameroot = 0;
    MarkSubgame(subgames[i], subgames[i]);
  }
}

void efgGame::UnmarkSubgames(Node *n)
{
  if (NumChildren(n) == 0)   return;

  for (int i = 1; i <= NumChildren(n); i++)
    UnmarkSubgames(n->GetChild(i));
  
  if (n->gameroot == n && n->parent)  {
    n->gameroot = 0;
    MarkSubgame(n, n->parent->gameroot);
  }
}


int efgGame::ProfileLength(void) const
{
  int sum = 0;

  for (int i = 1; i <= players.Length(); i++)
    for (int j = 1; j <= players[i]->infosets.Length(); j++)
      sum += players[i]->infosets[j]->actions.Length();

  return sum;
}

gArray<int> efgGame::NumInfosets(void) const
{
  gArray<int> foo(players.Length());
  
  for (int i = 1; i <= foo.Length(); i++) {
    foo[i] = players[i]->NumInfosets();
  }

  return foo;
}

int efgGame::NumPlayerInfosets() const
{
  int answer(0);
  int pl;
  for (pl = 1; pl <= NumPlayers(); pl++)
    answer +=  players[pl]->infosets.Length();
  return answer;
}

int efgGame::NumChanceInfosets() const
{
  return chance->infosets.Length();
}

int efgGame::TotalNumInfosets(void) const
{
  return NumPlayerInfosets() + NumChanceInfosets();
}

gPVector<int> efgGame::NumActions(void) const
{
  gArray<int> foo(players.Length());
  int i;
  for (i = 1; i <= players.Length(); i++)
    foo[i] = players[i]->infosets.Length();

  gPVector<int> bar(foo);
  for (i = 1; i <= players.Length(); i++) {
    for (int j = 1; j <= players[i]->infosets.Length(); j++) {
      bar(i, j) = players[i]->infosets[j]->NumActions();
    }
  }

  return bar;
}  

int efgGame::NumPlayerActions(void) const
{
  int answer = 0;

  gPVector<int> nums_actions = NumActions();
  for (int i = 1; i <= NumPlayers(); i++)
    answer += nums_actions[i];
  return answer;
}

int efgGame::NumChanceActions(void) const
{
  int answer = 0;

  for (int i = 1; i <= players[0]->infosets.Length(); i++) {
    answer += players[0]->infosets[i]->NumActions();
  }

  return answer;
}

gPVector<int> efgGame::NumMembers(void) const
{
  gArray<int> foo(players.Length());

  for (int i = 1; i <= players.Length(); i++) {
    foo[i] = players[i]->NumInfosets();
  }

  gPVector<int> bar(foo);
  for (int i = 1; i <= players.Length(); i++) {
    for (int j = 1; j <= players[i]->NumInfosets(); j++) {
      bar(i, j) = players[i]->infosets[j]->NumMembers();
    }
  }

  return bar;
}

//------------------------------------------------------------------------
//                       Efg: Payoff computation
//------------------------------------------------------------------------

void efgGame::Payoff(Node *n, gNumber prob, const gPVector<int> &profile,
		 gVector<gNumber> &payoff) const
{
  if (n->outcome)  {
    for (int i = 1; i <= players.Length(); i++)
      payoff[i] += prob * n->outcome->m_payoffs[i];
  }

  if (n->infoset && n->infoset->player->IsChance())
    for (int i = 1; i <= n->children.Length(); i++)
      Payoff(n->children[i],
	     prob * GetChanceProb(n->infoset, i),
	     profile, payoff);
  else if (n->infoset)
    Payoff(n->children[profile(n->infoset->player->number,n->infoset->number)],
	   prob, profile, payoff);
}

void efgGame::InfosetProbs(Node *n, gNumber prob, const gPVector<int> &profile,
			  gPVector<gNumber> &probs) const
{
  if (n->infoset && n->infoset->player->IsChance())
    for (int i = 1; i <= n->children.Length(); i++)
      InfosetProbs(n->children[i],
		   prob * GetChanceProb(n->infoset, i),
		   profile, probs);
  else if (n->infoset)  {
    probs(n->infoset->player->number, n->infoset->number) += prob;
    InfosetProbs(n->children[profile(n->infoset->player->number,n->infoset->number)],
		 prob, profile, probs);
  }
}

void efgGame::Payoff(const gPVector<int> &profile, gVector<gNumber> &payoff) const
{
  ((gVector<gNumber> &) payoff).operator=((gNumber) 0);
  Payoff(root, 1, profile, payoff);
}

void efgGame::InfosetProbs(const gPVector<int> &profile,
			  gPVector<gNumber> &probs) const
{
  ((gVector<gNumber> &) probs).operator=((gNumber) 0);
  InfosetProbs(root, 1, profile, probs);
}

void efgGame::Payoff(Node *n, gNumber prob, const gArray<gArray<int> *> &profile,
		    gArray<gNumber> &payoff) const
{
  if (n->outcome)   {
    for (int i = 1; i <= players.Length(); i++)
      payoff[i] += prob * n->outcome->m_payoffs[i];
  }
  
  if (n->infoset && n->infoset->player->IsChance())
    for (int i = 1; i <= n->children.Length(); i++)
      Payoff(n->children[i],
	     prob * GetChanceProb(n->infoset, i),
	     profile, payoff);
  else if (n->infoset)
    Payoff(n->children[(*profile[n->infoset->player->number])[n->infoset->number]],
	   prob, profile, payoff);
}

void efgGame::Payoff(const gArray<gArray<int> *> &profile,
		 gArray<gNumber> &payoff) const
{
  for (int i = 1; i <= payoff.Length(); i++)
    payoff[i] = 0;
  Payoff(root, 1, profile, payoff);
}

Nfg *efgGame::AssociatedNfg(void) const
{
  if (lexicon) {
    return lexicon->N;
  }
  else {
    return 0;
  }
}

Nfg *efgGame::AssociatedAfg(void) const
{
  return afg;
}
