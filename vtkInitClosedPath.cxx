#include "vtkInitClosedPath.h"
#include "MeshOps.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkCellLocator.h"
#include "vtkFloatArray.h"
#include "vtkMath.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkPolygon.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkTriangleFilter.h"
#include "vtkSmartPointer.h"

#include <list>
#include <vector>
#include <algorithm>

#include <iostream>

bool IsAllInitConnected( const std::vector< std::vector<int> > &LstarIJidx ) {
  size_t M = LstarIJidx.size();
  for( size_t i = 0; i < M; i++ ) {
    for( size_t j = 0; j < M; j++ ) {
      if( LstarIJidx[i][j] < 0.0 )
        return false;
    }
  }
  return true;
}

bool IsSquare( const std::vector< std::vector<int> >  &idxContactedInit ) {
  size_t M = idxContactedInit.size(); 
  for( size_t i = 0; i < M; i++ )
    {
    size_t N = (idxContactedInit[i]).size();
    if( N != M )
      {
      return false;
      }
    }
  return true;
}

bool IsMaxByOne( const std::vector< std::vector<int> > &idxContactedInit, int idx ) {
  size_t iMin = idxContactedInit.size();
//  size_t iMax = 0;
  for( size_t i = 0; i < iMin; i++ ) {
    if( (idxContactedInit[idx]).size( ) >=  ( 1+(idxContactedInit[i]).size() ) )
      return true;
  }
  return false;
}


int UpdateInitLists( const std::vector< std::vector<int> > LstarIJidx,
                     std::vector< std::vector<int> > accessible_init_indices ) {
    // now append to the initializers...
    int iIter = 0;
    int iterMax = LstarIJidx.size() * LstarIJidx.size();
    std::vector< std::vector<int> > bShowUpdate = LstarIJidx;
    bool bDidUpdate = true;
    while( iIter < iterMax && bDidUpdate ) {
     bDidUpdate = false;
     for( ::size_t i = 0; i < LstarIJidx.size(); i++ ) {
        for( ::size_t j = 0; j < LstarIJidx.size(); j++ ) {
          int idx    = LstarIJidx[i][j];
          /* As the new seed point is added, propogate the indices of
             initial point from where it was created */
          if( idx > -1 ) {
            for( ::size_t j2 = 0; j2 < LstarIJidx.size(); j2++ ) {
              int idxRev = LstarIJidx[i][j2];
              if( idxRev > -1 ) {
                // go to index at [i,j2]; add all of its init points to [i,j]'s index, if not already there
                std::vector<int> IJinitList = accessible_init_indices[ idxRev ];
                for( ::size_t m = 0; m < IJinitList.size(); m++ ) {
                  int init_pt = IJinitList[m];
                  if( 0 == std::count( accessible_init_indices[idx].begin(),
                    accessible_init_indices[idx].end(), init_pt) ) {
                        accessible_init_indices[idx].push_back( init_pt );
                        bShowUpdate[i][j] = -3;
                        bShowUpdate[i][j2] = -2;
                        bDidUpdate = true;
                  }
                }
              }
            }
          }
        }
      }
      iIter++;
    }
    return iIter;
}

// DEBUG_DISPLAY_0:
void Debug_Display_Path_Vals( vtkIntArray* activeContourVertIdx,  
                        const std::vector< std::vector<int> > &neigh_idx,
                        const std::vector< int > &seedIdx_ ) 
{
  ::size_t numVerts = neigh_idx.size();
  ::size_t numInit  = neigh_idx[0].size();
  
  std::vector<int> seedIdx = seedIdx_;
  std::sort( seedIdx.begin(), seedIdx.end() );
  seedIdx.erase( std::unique(seedIdx.begin(), seedIdx.end()),seedIdx.end() );

  for( ::size_t i = 0; i < numVerts; i++ ) {
      int val = 0;
      for( ::size_t m = 0; m < numInit; m++ ) {
        int idx = neigh_idx[i][m];
        if( idx >= 0 )
          val++;
      }
      activeContourVertIdx->SetValue( i, val );
  }
   for( ::size_t i = 0; i < seedIdx.size(); i++ ) {
        int val = -2; 
        activeContourVertIdx->SetTupleValue(  seedIdx[i], &val );
   }

}

void Debug_Display_Path_Vals2( vtkIntArray* activeContourVertIdx,  
                        const std::vector< std::vector<int> > &neigh_idx,
                        const std::vector< int > &seedIdx ) 
{
  int numVerts = neigh_idx.size();
//  int numInit  = neigh_idx[0].size();
  
  
  for( ::size_t m = 0; m < 1; m++ ) {
      // int prevIdx = seedIdx[m];
      int k = 0;
      while(k < numVerts ) {
        
        int idx = neigh_idx[ k ][m];
        int val = m;
        if( idx < 0 ) {
          val = -2; 
        }
        else {
          activeContourVertIdx->SetValue( k, val);
        }
        k++;
      }
  }

}


// DEBUG_DISPLAY_1:
void Debug_Display_L_Vals( vtkIntArray* activeContourVertIdx, 
                          const std::vector<double>& Lstar, const std::vector<int>& seedIdx,
                          const std::vector<int>& seedIdxPrv) 
{
      ::size_t numVerts = Lstar.size();
      for( ::size_t i = 0; i < numVerts; i++ ) {
        int val = ceil( Lstar[i] );
        activeContourVertIdx->SetTupleValue(i, &val);
      }
      for( ::size_t i = 0; i < seedIdx.size(); i++ ) {
        int val = -2; 
        activeContourVertIdx->SetTupleValue(  seedIdx[i], &val );
      }
      for( ::size_t i = 0; i < seedIdxPrv.size(); i++ ) {
        int val = 30; 
        activeContourVertIdx->SetTupleValue(  seedIdxPrv[i], &val );
      }
}

void Debug_Display_L_Blob( vtkIntArray* activeContourVertIdx, 
                          const std::vector<double>& Lstar, const std::vector<int>& seedIdx,
                          const std::vector<int>& seedIdxPrv) 
{
      ::size_t numVerts = Lstar.size();
      double sum = 0.0;
      double Lmin = 1e12;
      double Lmax = -1.0;
      for( ::size_t i = 0; i < numVerts; i++ ) {
        double val = ( Lstar[i] );
        sum += val;
        if( val < Lmin )
          Lmin = val;
        if( val > Lmax )
          Lmax = val;
      }
//      double Lmean = sum / numVerts;

      for( ::size_t i = 0; i < numVerts; i++ ) {
        int val = 0;
        double Lval = abs(Lstar[i]);
        double dThresh = 1e-3;
        if( Lval < dThresh )
          val = 1; // if we're 'close to the contour'     
        activeContourVertIdx->SetTupleValue(i, &val);
      }
}

// DEBUG_DISPLAY_2:
void Debug_Display_Connected_Vals( vtkIntArray* activeContourVertIdx, 
                             const std::vector<double>& Lstar, const std::vector<int>& seedIdx ) {
  ::size_t numVerts = Lstar.size();
      for( ::size_t i = 0; i < numVerts; i++ ) {
        int val = 0;
        activeContourVertIdx->SetTupleValue(i, &val);
      }
      for( ::size_t i = 0; i < seedIdx.size(); i++ ) {
        int val = 1; 
        activeContourVertIdx->SetTupleValue(  seedIdx[i], &val );
        //cout<<seedIdx[i]<<",";
      }
}


vtkCxxRevisionMacro(vtkInitClosedPath, "$Revision: 1.00 $");
vtkStandardNewMacro(vtkInitClosedPath);

vtkInitClosedPath::vtkInitClosedPath()
{
  
  //this->SetNumberOfInputPorts(2);
  // port 0: polyData
  
  
  // NO! use a SET method before calling
      // (deprecated) port 1: array of vertex points (sparse, initialization)

 // this->SetNumberOfOutputPorts(1);
  // port 0: polyData
  
  // NO! need it ???
    // (deprecated) port 1: vertex points (dense active contour)

  this->activeContourVertIdx = vtkSmartPointer<vtkIntArray>::New();
  this->activeContourSeedIdx = vtkSmartPointer<vtkIntArray>::New();
  this->bForceRecompute = false;
}

void vtkInitClosedPath::SetSource(vtkPolyData *source)
{
  this->SetInput(1, source);
}

vtkPolyData *vtkInitClosedPath::GetSource()
{
  if (this->GetNumberOfInputConnections(1) < 1)
    {
    return NULL;
    }
  return vtkPolyData::SafeDownCast(
    this->GetExecutive()->GetInputData(1, 0));
}

// Where the bulk of work is done!
int vtkInitClosedPath::RequestData(
  vtkInformation *vtkNotUsed(request),
  vtkInformationVector **inputVector,
  vtkInformationVector *outputVector)
{
   // get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
 // vtkInformation *sourceInfo = inputVector[1]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  
  // update progress bar at some intervals
  this->UpdateProgress(0.15);

    // get the input and ouptut
  vtkPolyData *input = vtkPolyData::SafeDownCast(
    inInfo->Get(vtkDataObject::DATA_OBJECT()));
//  vtkPolyData *source = 0;
  //if (sourceInfo) // This does nothing!
  //  {
  //  source = vtkPolyData::SafeDownCast(
  //    sourceInfo->Get(vtkDataObject::DATA_OBJECT()));
  //  }
  vtkPolyData *output = vtkPolyData::SafeDownCast(
    outInfo->Get(vtkDataObject::DATA_OBJECT()));
  
  
  // Check input
  ::size_t numVerts=input->GetNumberOfPoints();
  ::size_t numCells=input->GetNumberOfCells();
  if (numVerts < 1 || numCells < 1)
    {
    vtkErrorMacro(<<"No data to smooth!");
    return 1;
    }
  
  { // copy everything through first, before updating...
    output->CopyStructure(input);
    output->GetPointData()->PassData(input->GetPointData());
    output->GetCellData()->PassData(input->GetCellData());
    vtkDebugMacro(<<"Analyzing ____ ...");
  }
 
 
  vtkDataArray* contourIdxArrayIn = input->GetPointData()->GetArray("ActiveContourVertexIndices");
  if( bForceRecompute || (NULL == contourIdxArrayIn) )  { 

    
    size_t numInit ;
    std::vector<int> seedIdx; 

    if( 1 ) { // NULL == contourIdxArrayIn ) {
      activeContourVertIdx->SetName("ActiveContourVertexIndices");
      activeContourVertIdx->SetNumberOfComponents(1);
      activeContourVertIdx->SetNumberOfTuples( numVerts );
      numInit = this->activeContourSeedIdx->GetNumberOfTuples();
      seedIdx = std::vector<int>(numInit);
      for( ::size_t k = 0; k < numInit; k++ )
        seedIdx[k] = activeContourSeedIdx->GetValue(k);

    }
    else {
      activeContourVertIdx->DeepCopy( contourIdxArrayIn );
      seedIdx = std::vector<int>(0);
      for( ::size_t i = 0; i < numVerts; i++ ) {
//        int val = activeContourVertIdx->GetValue(i);
#define INIT_FLAG_VAL 0
        /*if( (val) == INIT_FLAG_VAL ) {
          seedIdx.push_back(i);
        } else */{
          activeContourVertIdx->SetValue( i, -1 );
        }
      }
      numInit = seedIdx.size();
    }
      
    // Now, link them together... 
    output->BuildLinks();
    vtkCellArray* faces = output->GetPolys();
    if (faces->GetNumberOfCells() == 0) {
      std::cerr << "No cells on the poly data polys\n";
      return 0;
    }

    std::vector<int> in_order_init_pts = std::vector<int>(0); 
        // list of points, in the 'traversal order' ! traversal order is decided by nearest-neighbor on mesh
        // i.e. the purpose of the following long chunk of code, propagating distance function between init nodes
    
    vtkSmartPointer<vtkIdList> cellIds = vtkSmartPointer<vtkIdList>::New();
    
    ::size_t numInit0 = numInit;
    std::vector< std::vector<int> > accessible_init_indices(numVerts);
    for( ::size_t i = 0; i < numVerts; i++ ) {
      accessible_init_indices[i] = std::vector<int>(numInit); // FIXED SIZE!
      for( ::size_t j = 0; j < numInit; j++ )
        accessible_init_indices[i][j] = -1; 
    }


    std::vector< std::vector<int> > idxContactedInit(numInit); // which other init points has this init point contacted while growing neighborhood
    for( ::size_t j = 0; j < numInit; j++) {
        idxContactedInit[j] = std::vector<int>(numInit);
        idxContactedInit[j][j] = 1;
    }

    for( ::size_t i = 0; i < numInit; i++ ) {
      int idx = seedIdx[i]; // index of initial points at mesh vertices
      accessible_init_indices[idx][i] = idx;
    }

    int recursions = 0;
    std::vector<int> seedIdxPrv = seedIdx;
BUILD_DISTANCE_TO_INIT:
    numInit = seedIdx.size();

   
    std::vector<int> firstHitInitIdx  = std::vector<int>(numVerts);
    std::list<int> nextVerts;
    std::vector<int> LstarIdx         = std::vector<int>(numVerts); // idx of current closest idx point
    std::vector<double> Lstar         = std::vector<double>(numVerts); // shortest distance to an init point
    std::vector< std::vector<int> > LstarIJidx; // idx for shortest distance to an init point
    std::vector< std::vector<double> > LstarIJval; // the val for shortest distance to init 
    std::vector< int > LstarIJbest; // closest adjacent seed;
    for( ::size_t i = 0; i < numVerts; i++ ) { 
      firstHitInitIdx[i] = -1;
      LstarIdx[i]        = -1;
      Lstar[i]           = -1.0;
    } 

   

    
    vtkIdType npts;
    vtkIdType* pts;
    //idxContactedInit.resize( numInit ); 
    LstarIJidx.resize( numInit );
    LstarIJval.resize( numInit );
    
    LstarIJbest.push_back( seedIdx[ 0 ] ); // first point of chosen initial curve!

//INIT_S_FUNC:
    for( size_t i = 0; i < numInit; i++ ) {
      //idxContactedInit[i].resize(1);
      LstarIJidx[i].resize(numInit);
      //(idxContactedInit[i])[0] = i;
      LstarIJval[i].resize(numInit);
      int idx = seedIdx[ i ];
      for( size_t j = 0; j < numInit; j++ ) {
        LstarIJidx[i][j]     = -1; 
        LstarIJval[i][j]     = -1.0;
      }
      (LstarIJidx[i])[i]      = idx;
      LstarIJval[i][i]        = 0.0;
      nextVerts.push_back( idx );
      firstHitInitIdx[idx] = i;
      LstarIdx[idx]        = i;
      Lstar[idx]           = 0; // 0 distance to itself
    }

    //for( size_t i = 0; i < numInit; i++ ) {
   //LOOP_BUILD_S_FUNC:
   // int idx_init_prev = nextVerts.fr
    while( (0 < nextVerts.size() ) )  // to-do list is not empty
    { // path not yet established between all pairs
      int idx = nextVerts.front( ); 
//      int idx_end = nextVerts.back( );
//      int idx_init = firstHitInitIdx[idx];
 
      output->GetPointCells( idx, cellIds ); // get cells with this index
      ::size_t iAdjCellCount = cellIds->GetNumberOfIds(); // how many cells are there with this idx?
      std::list<int> tmpNextVerts; tmpNextVerts.resize(0);
      for( ::size_t k = 0; k < iAdjCellCount; k++ )    {
        int id = cellIds->GetId( k ); // get the cell ID for k-th neighboring cell
        faces->GetCell(id*4, npts, pts ); // get "pts",  vertex indices ot neighbor cell
        for( int j = 0; j < npts; j++ ) {
          size_t pt = pts[j];
          if( 0 == std::count( tmpNextVerts.begin(), tmpNextVerts.end(), pt ) ) {
            int idxFirstHit = LstarIdx[pt]; //firstHitInitIdx[ pt ];
  //          int idxSpawn    = LstarIdx[idx]; //firstHitInitIdx[ idx ];
            bool bDidUpdate = false;
            if( -1 == idxFirstHit ) { // have not yet assigned it a first init point
              
                  //hasn't alrdy been added to Next, process it next round!
                nextVerts.push_back( pt );  
                firstHitInitIdx[ pt ] = firstHitInitIdx[idx];
                tmpNextVerts.push_back( pt );
                LstarIdx[pt] = LstarIdx[idx];
                double x1[3]; double x2[3];
                output->GetPoint( idx, x1 );
                output->GetPoint( pt, x2 );
                double deltaL = 0.0;
                for( ::size_t mm=0; mm<3; mm++ )
                  deltaL += pow( x1[mm]-x2[mm],2.0 );
                Lstar[pt]    = Lstar[idx]+sqrt(deltaL);
                bDidUpdate = true;
               
               /* std::vector<int> cur_idx   = accessible_init_indices[idx];
                std::vector<int> pt_idx    = accessible_init_indices[pt];
                for( ::size_t m = 0; m < cur_idx.size(); m++ ) {
                  int mval = accessible_init_indices[idx][m];
                  if( 0 == count( pt_idx.begin(), pt_idx.end(), mval ) )
                    accessible_init_indices[pt].push_back( mval );
                }*/
            }
            else if( 1/*pt != idx*/ ) {
                double x1[3]; double x2[3];
                output->GetPoint( idx, x1 );
                output->GetPoint( pt, x2 );
                double deltaL = 0.0;
                for( ::size_t mm=0; mm<3; mm++ )
                  deltaL += pow( x1[mm]-x2[mm],2.0 );
                double Lstar_    = Lstar[idx]+sqrt(deltaL);
                double Lstar_rev = Lstar[pt]+sqrt(deltaL);
                if( Lstar[pt]<0 || Lstar_ < Lstar[pt] ) {
                  // re-assign index pt to class at index idx
                  LstarIdx[pt] = LstarIdx[idx];
                  Lstar[pt]    = Lstar_;
                  nextVerts.push_back( pt );
            
                }
                else if( Lstar[idx]<0 || Lstar_rev < Lstar[idx] ) {
                  // re-assign index idx to class at index pt
                  LstarIdx[idx] = LstarIdx[pt];
                  Lstar[idx]    = Lstar_rev;
                  nextVerts.push_back( idx );
  
                }
                else // the neighbor has different class, and closer to someone else -> so we are at boundary!
                {
                 
                  // LstarIdx: list of assignments of each mesh vertex to
                  // the index of the seed point to which it is closest in
                  // terms of distance Lstar
                  int ii = LstarIdx[idx]; // one index into current list of seed points
                  int jj = LstarIdx[pt];  // another index into current seed points
                                        // seeds ii and jj are neighbors and
                                        // contact eachother at mesh indices idx,pt
                  double Lstar_ii_jj     = Lstar_; //+ Lstar[pt]; 
                  double Lstar_ii_jj_rev = Lstar_rev; //+ Lstar[idx];
                  // This part is where confusion creeps in ...
                  {
                    if( LstarIJval[ii][jj] > Lstar_ii_jj || LstarIJval[ii][jj] < 0 ) 
                    {
                      // Assign idx's neighbor to be pt (mesh indices)
                      // where pt is a mesh index that is closest to init point jj
                      LstarIJval[ii][jj]        = Lstar_ii_jj;
                      (LstarIJidx[ii])[jj]      = idx;
                    }
                    if( LstarIJval[jj][ii] > Lstar_ii_jj_rev || LstarIJval[jj][ii] < 0 ) 
                    {
                      // Assign pt's neighbor to be idx (mesh indices)
                      // where idx is a mesh index that is closest to init point ii
                      LstarIJval[jj][ii]        = Lstar_ii_jj_rev;
                      (LstarIJidx[jj])[ii]      = pt;
                    }
                    // presumably, if we did the above, pt and idx are neighbors...
                  }
                }

            }
            if( 1 ) {
              for( ::size_t m = 0; m < numInit0; m++ ) {
                int mval = accessible_init_indices[idx][m];
                if( mval > 0 ) { 
                  accessible_init_indices[pt][m] = idx;
                  for( ::size_t ck = 0; ck < numInit0; ck++ ) {
                    int idxRev = accessible_init_indices[mval][ck];
                    if( idxRev >= 0 ) {
                      idxContactedInit[m][ck] = 1;
                    }
                  }
                }
              }
              for( ::size_t m = 0; m < numInit0; m++ ) {
                int mval = accessible_init_indices[pt][m];
                if( mval > 0 ) { 
                  accessible_init_indices[idx][m] = pt;
                  for( ::size_t ck = 0; ck < numInit0; ck++ ) {
                    int idxRev = accessible_init_indices[mval][ck];
                    if( idxRev >= 0 ) {
                      idxContactedInit[m][ck] = 1;
                    }
                  } 
                }
              }
            }         
          }   
        } 
      }
      
      /* If all init points have come into contact, then stop and continue... */
      bool bAllConnected = true;
      for( ::size_t m = 0; m < numInit0; m++ ) {
        for( ::size_t n = 0; n < numInit0; n++ ) {
          if( idxContactedInit[m][n] == 0 ) {
            bAllConnected = false;
          }
        }
      }
      if( bAllConnected ) {
        nextVerts.clear();
        recursions = 1024;
      }
      else {
        nextVerts.pop_front();
      }
    }

     output->BuildLinks();  
     std::vector<AdjData> adjimm = std::vector<AdjData>(numVerts); 
     
     // for every face, make all vertices on the face store in the adjimm list
      for(unsigned int i = 0; i < numVerts; i++ ) {
        adjimm[i].myNeighbs    = std::vector<int>(1);
        adjimm[i].myNeighbs[0] = i;
        adjimm[i].myIdx = i;
      }

      int numfaces = faces->GetNumberOfCells();
      for( int i = 0; i < numfaces; i++ ) {
        vtkIdType npts;
        vtkIdType* pts;
        faces->GetCell(i*4,npts, pts );
//        int vert0 = pts[0];
//        int vert1 = pts[1];
//        int vert2 = pts[2];
        for( int k = 0 ; k < 3; k++ ) {
          for( int kk = 0; kk < 3; kk++ ) {
            if( 0 == std::count( adjimm[pts[kk]].myNeighbs.begin(), adjimm[pts[kk]].myNeighbs.end(),pts[k] ) ) {
              adjimm[pts[kk]].myNeighbs.push_back( pts[k] );
            }
          }
        }
      }

    std::vector<int> C = seedIdx;

    // now append to the initializers...
  #define NUM_INIT_RECURSIONS 2

    std::vector<int> seedIdxCopy (seedIdx);
    std::vector<int> seedIdxCopyCnt;
    this->connectedEdge.clear();
    this->connectedEdge.resize( seedIdx.size() );
    this->connectedCircuit.clear();
    this->connectedCircuit.resize( seedIdx.size() );
    for (int i = 0; i < seedIdx.size(); i ++) {
      seedIdxCopyCnt.push_back(2);
      for (int j = 0; j < seedIdx.size(); j ++) {
        this->connectedCircuit[i].push_back(false);
        this->connectedEdge[i].push_back(false);
      }
    }

    // implement a Cheapest Path Algorithm (CPA) to find a Hamilton circuit
    {

      while ( !seedIdxCopy.empty() ) 
      {
        int size = seedIdxCopy.size();

        // debug display
        cout << "size: " << size << endl;
        for (int i = 0; i < size; i ++) {
          cout << seedIdxCopyCnt[i] << endl;
        }

        double pt1[3];
        double pt2[3];
        double sinkPt[3];

        // use A* algorithm to find path between two seeds
        std::vector< std::vector< std::vector<int> > > history_idx (size);
        std::vector< std::vector< std::vector<int> > > frontier_idx (size);
        std::vector< std::vector< std::vector<double> > > frontier_val (size);
        std::vector< std::vector< std::vector< std::vector<int> > > > frontier_path (size);
        std::vector<int> path;
        
        // initialization...
        for (int i = 0; i < size; i++) {
          history_idx[i].resize(size);
          frontier_path[i].resize(size);
          frontier_idx[i].resize(size);
          frontier_val[i].resize(size);
          for (int j = 0; j < size; j++) {
            frontier_path[i][j].push_back(path);
            frontier_idx[i][j].push_back(-1);
            frontier_val[i][j].push_back(0.0);
          }
        }

        int bestSinkId;
        int bestSinkII;
        int bestSourceId;
        int bestSourceJJ;

        int count = 1; // for debug
        while (1) 
        {
          std::cout << "A* round " << count << endl;
          count ++;
          // std::cout << "find the best point at frontier" << endl;

          std::vector< std::vector<double> > minAStarVal (size);
          std::vector< std::vector<int> > minAStarIdx (size);
          std::vector< std::vector<int> > minAStarVecIdx (size);
          for (int i = 0; i < size; i++) {
            minAStarIdx[i].resize(size);
            minAStarVecIdx[i].resize(size);
            for (int j = 0; j < size; j++) {
              minAStarVal[i].push_back(1e9);
            }
          }
          for (int jj = 0; jj < size; jj++) {
            int idxSource = seedIdxCopy[jj];
            int iiSource;
            for (int i = 0; i < seedIdx.size(); i++) {
              if (idxSource == seedIdx[i])
                iiSource = i;
            }

            for (int ii = 0; ii < size; ii++) {
              if (frontier_idx[jj][ii].size() == 1 && frontier_idx[jj][ii][0] == -1) {
                frontier_idx[jj][ii][0] = idxSource;
              }
              int idxSink = seedIdxCopy[ii];
              int jjSink;
              for (int i = 0; i < seedIdx.size(); i++) {
                if (idxSink == seedIdx[i])
                  jjSink = i;
              }

              // test if form closed sub circuit
              bool haveSubCircuit = this->haveConnectedCircuit (iiSource, jjSink);
              // cout << "haveSubCircuit: " << haveSubCircuit << endl;

              if (idxSource != idxSink && !(haveSubCircuit && size > 2) )
              // avoid generating a sub circuit
              {
                output->GetPoint( idxSink, sinkPt ); // point where we're path finding towards

                // find the "best" point at frontier
                for (int i = 0; i < frontier_idx[jj][ii].size(); i++) {
                  output->GetPoint( frontier_idx[jj][ii][i], pt1 );
                  double dist = sqrt( pow(sinkPt[0]-pt1[0],2.0)+pow(sinkPt[1]-pt1[1],2.0)+pow(sinkPt[2]-pt1[2],2.0) );
                  double AStarVal = frontier_val[jj][ii][i] + dist;

                  if (AStarVal < minAStarVal[jj][ii]) {
                    minAStarVal[jj][ii] = AStarVal;
                    minAStarIdx[jj][ii] = frontier_idx[jj][ii][i];
                    minAStarVecIdx[jj][ii] = i;
                  }
                }
                // std::cout << "update frontier" << endl;
                // update frontier
                if (minAStarIdx[jj][ii] == idxSink) {
                  // std::cout << "put path into C" << endl;
                  path = frontier_path[jj][ii][minAStarVecIdx[jj][ii]];
                  for (int k = 0; k < path.size(); k++) {
                    C.push_back(path[k]);
                  }
                  bestSourceId = idxSource;
                  bestSourceJJ = jj;
                  bestSinkId = idxSink;
                  bestSinkII = ii;
                  goto loopend;
                } else {
                  // std::cout << "update" << endl;
                  for (int j = 0; j < adjimm[minAStarIdx[jj][ii]].myNeighbs.size(); j++) {
                    int newIdx = adjimm[minAStarIdx[jj][ii]].myNeighbs[j];
                    if ( std::count( history_idx[jj][ii].begin(), history_idx[jj][ii].end(), newIdx ) == 0 
                          && std::count( frontier_idx[jj][ii].begin(), frontier_idx[jj][ii].end(), newIdx ) == 0 ) 
                    {
                      // std::cout << "update idx" << endl;
                      // update idx
                      frontier_idx[jj][ii].push_back(newIdx);

                      // std::cout << "update val" << endl;
                      // update val
                      output->GetPoint( frontier_idx[jj][ii][minAStarVecIdx[jj][ii]], pt1 );
                      output->GetPoint( newIdx, pt2 );
                      double newDist = frontier_val[jj][ii][minAStarVecIdx[jj][ii]] + sqrt( pow(pt2[0]-pt1[0],2.0)+pow(pt2[1]-pt1[1],2.0)+pow(pt2[2]-pt1[2],2.0) );
                      frontier_val[jj][ii].push_back(newDist);

                      // std::cout << "update path" << endl;
                      // update path
                      path = frontier_path[jj][ii][minAStarVecIdx[jj][ii]];
                      path.push_back(newIdx);
                      frontier_path[jj][ii].push_back(path);
                    }
                  }
                  // std::cout << "put current idx into history" << endl;
                  // put current idx into history
                  history_idx[jj][ii].push_back(minAStarIdx[jj][ii]);
                  frontier_idx[jj][ii].erase(frontier_idx[jj][ii].begin() + minAStarVecIdx[jj][ii]);
                  frontier_val[jj][ii].erase(frontier_val[jj][ii].begin() + minAStarVecIdx[jj][ii]);
                  frontier_path[jj][ii].erase(frontier_path[jj][ii].begin() + minAStarVecIdx[jj][ii]);
                }
              }
            }
          }
        }
        loopend:

        // put this edge into connectedEdge
        int iiSource;
        int jjSink;
        for (int i = 0; i < seedIdx.size(); i ++) {
          if (bestSinkId == seedIdx[i])
            jjSink = i;
          if (bestSourceId == seedIdx[i])
            iiSource = i;
        }
        this->connectedEdge[iiSource][jjSink] = true;

        // update seedIdxCopyCnt & seedIdxCopy
        seedIdxCopyCnt[bestSinkII] --;
        seedIdxCopyCnt[bestSourceJJ] --;
        // delete zero cnt seed
        for (int i = 0; i < size; i ++) {
          if (seedIdxCopyCnt[i] <= 0) {
            seedIdxCopyCnt.erase( seedIdxCopyCnt.begin() + i );
            seedIdxCopy.erase( seedIdxCopy.begin() + i );
            i --;
            size --;
          }
        }
        std::cout << "round over" << endl;
      }
    }
    
    seedIdx = C;
    for( ::size_t m = 0; m < C.size(); m++ ) {
      int idx = C[m];
      Lstar[idx] = 0.0;
    }

    for( ::size_t m = 0; m < numVerts; m++ ) {
    //  accessible_init_indices[m][k] = idx;
      for( ::size_t n = 0; n < numInit0; n++ ) {
        int idx     = accessible_init_indices[m][n];
        if( idx >= 0 ) {
          int idxNext = accessible_init_indices[idx][n];
          if( idxNext >= 0 ) {
            accessible_init_indices[idxNext][n] = idx;
          }
        }
      }
    }

    for( ::size_t m = 0; m < numVerts; m++ ) {
      for( ::size_t n = 0; n < numInit0; n++ ) {
        int idx     = accessible_init_indices[m][n];
        for( ::size_t ck = 0; ck < numInit0; ck++ ) {
          int idxRev = accessible_init_indices[m][ck];
          if( idx >= 0 && idxRev >= 0 ) {
            idxContactedInit[n][ck] = 1;
//            int breakhere = 1;
          }
        }
      }
    }


//    int numInitNew = seedIdx.size();
//    bool bDoneSquare = 0; //IsSquare( idxContactedInit );
    if( seedIdx.size() != numInit && 
              recursions < NUM_INIT_RECURSIONS ) {
      recursions++;
      goto  BUILD_DISTANCE_TO_INIT;
    }
    else {
      std::cout<<"Num Seeds Generated: "<<seedIdx.size()<<"\n";
    }
    
    for( int n = 0; n < 0; n++ ) {
      std::vector<double> Lstar_ = Lstar;
      for( ::size_t k = 0; k < numVerts; k++ ) {
        std::vector<int>* neigh = &(adjimm[k].myNeighbs);
//        double sumLstar = 0.0;
        for( ::size_t m = 0 ; m < neigh->size(); m++ ) {
          Lstar[k] = Lstar[k] * Lstar_[ (*neigh)[m] ]; 
                // goes to zero if neighbor was zero
        }
      }
    }

     
    // Temp Debug Display: contour idx as color map
    //Debug_Display_L_Vals( activeContourVertIdx, Lstar, seedIdx, seedIdxPrv );
    Debug_Display_L_Blob( activeContourVertIdx, Lstar, seedIdx, seedIdxPrv );

    output->GetPointData()->AddArray( activeContourVertIdx );
  }
  else { // if it already exists, verify that we point to it
     activeContourVertIdx = vtkIntArray::SafeDownCast( contourIdxArrayIn );
     std::string name( activeContourVertIdx->GetName( ) );
     std::cout<<"re-using existing array named: "<<name<<"\n";
  }

  // update progress bar at some intervals
  this->UpdateProgress(1.0);
 
  return 1;
 }
  
int vtkInitClosedPath::FillInputPortInformation(int port,
                                                      vtkInformation *info)
{
  if (!this->Superclass::FillInputPortInformation(port, info))
    {
    return 0;
    }
  
  if (port == 1)
    {
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
    }
  return 1;
}


void vtkInitClosedPath::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "Truth: " << 42 << "\n";
  if ( this->GetSource() )
    {
      os << indent << "Source: " << static_cast<void *>(this->GetSource()) << "\n";
    }
  else
    {
    os << indent << "Source (none)\n";
    }
}
