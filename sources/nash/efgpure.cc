//
// $Source$
// $Date$
// $Revision$
//
// DESCRIPTION:
// Compute pure-strategy equilibria in extensive form games
//
// This file is part of Gambit
// Copyright (c) 2002, The Gambit Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//

#include "base/base.h"
#include "efgpure.h"

#include "game/game.h"
#include "game/efgiter.h"
#include "game/efgciter.h"
#include "behavsol.h"

gbtList<BehavSolution> gbtEfgNashEnumPure::Solve(const gbtEfgSupport &p_support,
					       gbtStatus &p_status)
{
  gbtList<BehavSolution> solutions;

  gbtEfgContIterator citer(p_support);
  gbtPVector<gbtNumber> probs(p_support->NumInfosets());

  int ncont = 1;
  for (int pl = 1; pl <= p_support->NumPlayers(); pl++) {
    gbtGamePlayer player = p_support->GetPlayer(pl);
    for (int iset = 1; iset <= player->NumInfosets(); iset++) {
      ncont *= p_support->NumActions(pl, iset);
    }
  }

  int contNumber = 1;
  try {
    do  {
      p_status.Get();
      p_status.SetProgress((double) contNumber / (double) ncont);

      bool flag = true;
      citer.GetProfile().InfosetProbs(probs);

      gbtEfgIterator eiter(citer);
      
      for (int pl = 1; flag && pl <= p_support->NumPlayers(); pl++)  {
	gbtNumber current = citer.Payoff(pl);
	for (int iset = 1;
	     flag && iset <= p_support->GetPlayer(pl)->NumInfosets();
	     iset++)  {
	  if (probs(pl, iset) == gbtNumber(0))   continue;
	  for (int act = 1; act <= p_support->NumActions(pl, iset); act++)  {
	    eiter.Next(pl, iset);
	    if (eiter.Payoff(pl) > current)  {
	      flag = false;
	      break;
	    }
	  }
	}
      }

      if (flag)  {
	gbtBehavProfile<gbtNumber> temp = p_support->NewBehavProfile(gbtNumber(0));
	// zero out all the entries, since any equilibria are pure
	((gbtVector<gbtNumber> &) temp).operator=(gbtNumber(0));
	const gbtEfgContingency &profile = citer.GetProfile();
	for (gbtGamePlayerIterator player(p_support->GetTree());
	     !player.End(); player++) {
	  for (gbtGameInfosetIterator infoset(*player);
	       !infoset.End(); infoset++) {
	    temp((*player)->GetId(),
		 (*infoset)->GetId(),
		 profile.GetAction(*infoset)->GetId()) = 1;
	  }
	}

	solutions.Append(BehavSolution(temp, "EnumPure[EFG]"));
      }
      contNumber++;
    }  while ((m_stopAfter == 0 || solutions.Length() < m_stopAfter) &&
	      citer.NextContingency());
  }
  catch (gbtSignalBreak &) {
    // catch exception; return list of computed equilibria (if any)
  }

  return solutions;
}
