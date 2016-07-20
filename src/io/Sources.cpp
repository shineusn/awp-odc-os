/**
 @brief Reads input source files and sets up data structures to store fault node rupture information.
 
 @section LICENSE
 Copyright (c) 2015-2016, Regents of the University of California
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// TODO: Provide non-mpi version.
#include <mpi.h>
#include <stdio.h>
#include "constants.hpp"
#include "io/Sources.hpp"
#include "parallel/Mpi.hpp"
#include "data/Grid.hpp"

odc::io::Sources::Sources(int   i_iFault,
                 int   i_nSrc,
                 int   i_readStep,
                 int   i_nSt,
                 int   i_nZ,
                 int   i_nXt, int i_nYt, int i_nZt,
                 char *i_inSrc,
                 char *i_inSrcI2 ) {
    
    
    inisource(0, i_iFault, i_nSrc, i_readStep, i_nSt, &m_srcProc, i_nZ,
              MPI_COMM_WORLD, i_nXt, i_nYt, i_nZt, odc::parallel::Mpi::coords,
              3, &m_nPsrc, &m_ptpSrc,&m_ptAxx, &m_ptAyy, &m_ptAzz, &m_ptAxz,
              &m_ptAyz, &m_ptAxy, i_inSrc, i_inSrcI2 );
    
}


/**
 Reads fault source file and sets corresponding parameter values for source fault nodes owned by the calling process when @c IFAULT==2
 
 @param rank            Rank of calling process (from MPI)
 @param READ_STEP       Number of rupture timesteps to read from source file
 @param INSRC           Prefix of source input file
 @param INSRC_I2        Split source input file prefix for @c IFAULT==2 option
 @param maxdim          Number of spatial dimensions (always 3)
 @param coords          @c int array (length 2) that stores the x and y coordinates of calling process in the Cartesian MPI topology
 @param NZ              z model dimension in nodes
 @param nxt             Number of x nodes that calling process owns
 @param nyt             Number of y nodes that calling process owns
 @param nzt             Number of z nodes that calling process owns
 
 
 @param[out] NPSRC           Number of fault source nodes owned by calling process
 @param[out] SRCPROC         Set to rank of calling process (or -1 if source file could not be opened)
 @param[out] psrc            Array of length <tt>NPSRC*maxdim</tt> that stores indices of nodes owned by calling process that are fault sources \n
                                    <tt>psrc[i*maxdim]</tt> = x node index of source fault @c i \n
                                    <tt>psrc[i*maxdim+1]</tt> = y node index of source fault @c i \n
                                    <tt>psrc[i*maxdim+2]</tt> = z node index of source fault @c i
 @param[out] axx
 @param[out] ayy
 @param[out] azz
 @param[out] axz
 @param[out] ayz
 @param[out] axy
 @param[out] idx
 */
int read_src_ifault_2(int rank, int READ_STEP,
                      char *INSRC, char *INSRC_I2,
                      int maxdim, int *coords, int NZ,
                      int nxt, int nyt, int nzt,
                      int *NPSRC, int *SRCPROC,
                      PosInf *psrc, Grid1D *axx, Grid1D *ayy, Grid1D *azz,
                      Grid1D *axz, Grid1D *ayz, Grid1D *axy,
                      int idx){
    
    FILE *f;
    char fname[150];
    int dummy[2], i, j;
    int nbx, nby;
    PosInf tpsrc = NULL;
    Grid1D taxx=NULL, tayy=NULL, tazz=NULL;
    Grid1D taxy=NULL, taxz=NULL, tayz=NULL;
    
    // First time entering this function
    if(idx == 1){
        //TODO: unsafe call; this should be snprintf, or fname should be dynamically allocated to always be large enough to hold the format string
        sprintf(fname, "%s%07d", INSRC, rank);
        printf("SOURCE reading first time: %s\n",fname);
        f = fopen(fname, "rb");
        if(f == NULL){
            printf("SOURCE %d) no such file: %s\n",rank,fname);
            *SRCPROC = -1;
            *NPSRC = 0;
            return 0;
        }
        *SRCPROC = rank;
        nbx     = nxt*coords[0] + 1 - 2;
        nby     = nyt*coords[1] + 1 - 2;
        // not sure what happens if maxdim != 3
        fread(NPSRC, sizeof(int), 1, f);
        fread(dummy, sizeof(int), 2, f);
        
        printf("SOURCE I am, rank=%d npsrc=%d\n",rank,*NPSRC);
        
        tpsrc = odc::data::Alloc1P((*NPSRC)*maxdim);
        fread(tpsrc, sizeof(int), (*NPSRC)*maxdim, f);
        // assuming nzt=NZ
        for(i=0; i<*NPSRC; i++){
            //tpsrc[i*maxdim] = (tpsrc[i*maxdim]-1)%nxt+1;
            //tpsrc[i*maxdim+1] = (tpsrc[i*maxdim+1]-1)%nyt+1;
            //tpsrc[i*maxdim+2] = NZ+1 - tpsrc[i*maxdim+2];
            tpsrc[i*maxdim]   = tpsrc[i*maxdim] - nbx - 1;
            tpsrc[i*maxdim+1] = tpsrc[i*maxdim+1] - nby - 1;
            tpsrc[i*maxdim+2] = NZ + 1 - tpsrc[i*maxdim+2];
        }
        *psrc = tpsrc;
        fclose(f);
    }
    if(*NPSRC > 0){
        //TODO: unsafe call; this should be snprintf, or fname should be dynamically allocated to always be large enough to hold the format string
        sprintf(fname, "%s%07d_%03d", INSRC_I2, rank, idx);
        printf("SOURCE reading: %s\n",fname);
        f = fopen(fname, "rb");
        if(f == NULL){
            printf("ERROR! Rank=%d: Cannot open file: %s\n", rank, fname);
            return -1;
        }
        // fastest axis in partitioned source is npsrc, then read_step (see source.f)
        // fastest axis in axx is time
        Grid1D tmpta;
        tmpta = odc::data::Alloc1D((*NPSRC)*READ_STEP);
        if(idx==1){
            taxx = odc::data::Alloc1D((*NPSRC)*READ_STEP);
            tayy = odc::data::Alloc1D((*NPSRC)*READ_STEP);
            tazz = odc::data::Alloc1D((*NPSRC)*READ_STEP);
            taxy = odc::data::Alloc1D((*NPSRC)*READ_STEP);
            taxz = odc::data::Alloc1D((*NPSRC)*READ_STEP);
            tayz = odc::data::Alloc1D((*NPSRC)*READ_STEP);
        }
        else{
            taxx = *axx;
            tayy = *ayy;
            tazz = *azz;
            taxy = *axy;
            taxz = *axz;
            tayz = *ayz;
        }
        fread(tmpta, sizeof(float), (*NPSRC)*READ_STEP, f);
        for(i=0; i<*NPSRC; i++)
            for(j=0; j<READ_STEP; j++)
                taxx[i*READ_STEP+j] = tmpta[j*(*NPSRC)+i];
        fread(tmpta, sizeof(float), (*NPSRC)*READ_STEP, f);
        for(i=0; i<*NPSRC; i++)
            for(j=0; j<READ_STEP; j++)
                tayy[i*READ_STEP+j] = tmpta[j*(*NPSRC)+i];
        fread(tmpta, sizeof(float), (*NPSRC)*READ_STEP, f);
        for(i=0; i<*NPSRC; i++)
            for(j=0; j<READ_STEP; j++)
                tazz[i*READ_STEP+j] = tmpta[j*(*NPSRC)+i];
        fread(tmpta, sizeof(float), (*NPSRC)*READ_STEP, f);
        for(i=0; i<*NPSRC; i++)
            for(j=0; j<READ_STEP; j++)
                taxz[i*READ_STEP+j] = tmpta[j*(*NPSRC)+i];
        fread(tmpta, sizeof(float), (*NPSRC)*READ_STEP, f);
        for(i=0; i<*NPSRC; i++)
            for(j=0; j<READ_STEP; j++)
                tayz[i*READ_STEP+j] = tmpta[j*(*NPSRC)+i];
        fread(tmpta, sizeof(float), (*NPSRC)*READ_STEP, f);
        for(i=0; i<*NPSRC; i++)
            for(j=0; j<READ_STEP; j++)
                taxy[i*READ_STEP+j] = tmpta[j*(*NPSRC)+i];
        fclose(f);
        *axx = taxx;
        *ayy = tayy;
        *azz = tazz;
        *axz = taxz;
        *ayz = tayz;
        *axy = taxy;
        odc::data::Delloc1D(tmpta);
    }
    
    return 0;
}


/**
 Reads fault source file and sets corresponding parameter values for source fault nodes owned by the calling process
 
 @bug When @c IFAULT==1, @c READ_STEP does not work. It reads all the time steps at once instead of only @c READ_STEP of them.
 
 @param rank        Rank of calling process (from MPI)
 @param IFAULT      Mode selection and fault or initial stress setting (1 or 2)
 @param NSRC        Number of source nodes on fault
 @param READ_STEP   Number of rupture timesteps to read from source file
 @param NST         Number of time steps in rupture functions
 @param NZ          z model dimension in nodes
 @param MCW         MPI process communicator for 2D Cartesian MPI Topology
 @param nxt         Number of x nodes that calling process owns
 @param nyt         Number of y nodes that calling process owns
 @param nzt         Number of z nodes that calling process owns
 @param coords      @c int array (length 2) that stores the x and y coordinates of calling process in the Cartesian MPI topology
 @param maxdim      Number of spatial dimensions (always 3)
 @param INSRC       Source input file (if @c IFAULT==2, then this is prefix of @c tpsrc)
 @param INSRC_I2    Split source input file prefix for @c IFAULT==2 option
 
 @param[out] SRCPROC     If calling process owns one or more fault source nodes, @c SRCPROC is set to rank of calling process (MPI). If calling process
 does not own any fault source nodes @c SRCPROC is set to -1
 @param[out] NPSRC       Number of fault source nodes owned by calling process
 @param[out] ptpsrc      Array of length <tt>NPSRC*maxdim</tt> that stores indices of nodes owned by calling process that are fault sources \n
 <tt>ptpsrc[i*maxdim]</tt> = x node index of source fault @c i \n
 <tt>ptpsrc[i*maxdim+1]</tt> = y node index of source fault @c i \n
 <tt>ptpsrc[i*maxdim+2]</tt> = z node index of source fault @c i
 @param[out] ptaxx       Array of length <tt>NPSRC*READ_STEP</tt> that stores fault rupture function value (2nd x partial) \n
 <tt>ptaxx[i*READ_STEP + j]</tt> = rupture function for source fault @c i, value at step @c j (where <tt>0 <= j <= READ_STEP</tt>)
 @param[out] ptayy       Array of length <tt>NPSRC*READ_STEP</tt> that stores fault rupture function value (2nd y partial) \n
 <tt>ptayy[i*READ_STEP + j]</tt> = rupture function for source fault @c i, value at step @c j (where <tt>0 <= j <= READ_STEP</tt>)
 @param[out] ptazz       Array of length <tt>NPSRC*READ_STEP</tt> that stores fault rupture function value (2nd z partial) \n
 <tt>ptazz[i*READ_STEP + j]</tt> = rupture function for source fault @c i, value at step @c j (where <tt>0 <= j <= READ_STEP</tt>)
 @param[out] ptaxz       Array of length <tt>NPSRC*READ_STEP</tt> that stores fault rupture function value (mixed xz partial) \n
 <tt>ptaxz[i*READ_STEP + j]</tt> = rupture function for source fault @c i, value at step @c j (where <tt>0 <= j <= READ_STEP</tt>)
 @param[out] ptayz       Array of length <tt>NPSRC*READ_STEP</tt> that stores fault rupture function value (mixed yz partial) \n
 <tt>ptayz[i*READ_STEP + j]</tt> = rupture function for source fault @c i, value at step @c j (where <tt>0 <= j <= READ_STEP</tt>)
 @param[out] ptaxy       Array of length <tt>NPSRC*READ_STEP</tt> that stores fault rupture function value (mixed xy partial) \n
 <tt>ptaxy[i*READ_STEP + j]</tt> = rupture function for source fault @c i, value at step @c j (where <tt>0 <= j <= READ_STEP</tt>)
 
 @return 0 on success, -1 if there was an error when reading input files
 */
int inisource(int      rank,    int     IFAULT, int     NSRC,   int     READ_STEP, int     NST,     int     *SRCPROC, int    NZ,
              MPI_Comm MCW,     int     nxt,    int     nyt,    int     nzt,       int     *coords, int     maxdim,   int    *NPSRC,
              PosInf   *ptpsrc, Grid1D  *ptaxx, Grid1D  *ptayy, Grid1D  *ptazz,    Grid1D  *ptaxz,  Grid1D  *ptayz,   Grid1D *ptaxy, char *INSRC, char *INSRC_I2)
{
    int i, j, k, npsrc, srcproc, master=0;
    int nbx, nex, nby, ney, nbz, nez;
    PosInf tpsrc=NULL, tpsrcp =NULL;
    Grid1D taxx =NULL, tayy   =NULL, tazz =NULL, taxz =NULL, tayz =NULL, taxy =NULL;
    Grid1D taxxp=NULL, tayyp  =NULL, tazzp=NULL, taxzp=NULL, tayzp=NULL, taxyp=NULL;
    if(NSRC<1) return 0;
    
    npsrc   = 0;
    srcproc = -1;
    
    // Calculate starting and ending x, y, and z node indices that are owned by calling process. Since MPI topology is 2D each process owns every z node.
    // Indexing is based on 1: [1, nxt], etc. Include 1st layer ghost cells ("loop" is defined to be 1)
    //
    // [        -         -       |                  - . . . -                            |          -       -              ]
    // ^        ^                 ^                      ^                                ^                 ^               ^
    // |        |                 |                      |                                |                 |               |
    // nbx    2 ghost cells     nbx+2       regular cells (nxt of them)             nbx+(nxt-1)+2       2 ghost cells       nex
    nbx     = nxt*coords[0] + 1 - 2;
    nex     = nbx + nxt - 1;
    nby     = nyt*coords[1] + 1 - 2;
    ney     = nby + nyt - 1;
    nbz     = 1;
    nez     = nzt;
    
    // IFAULT=1 has bug! READ_STEP does not work, it tries to read NST all at once - Efe
    if(IFAULT<=1)
    {
        // Source node of rupture
        tpsrc = odc::data::Alloc1P(NSRC*maxdim);
        
        // Rupture function values (2nd order partials)
        taxx  = odc::data::Alloc1D(NSRC*READ_STEP);
        tayy  = odc::data::Alloc1D(NSRC*READ_STEP);
        tazz  = odc::data::Alloc1D(NSRC*READ_STEP);
        taxz  = odc::data::Alloc1D(NSRC*READ_STEP);
        tayz  = odc::data::Alloc1D(NSRC*READ_STEP);
        taxy  = odc::data::Alloc1D(NSRC*READ_STEP);
        
        // Read rupture function data from input file
        if(rank==master)
        {
            FILE   *file;
            int    tmpsrc[3];
            Grid1D tmpta;
            if(IFAULT == 1){
                file = fopen(INSRC,"rb");
                tmpta = odc::data::Alloc1D(NST*6);
            }
            else if(IFAULT == 0) file = fopen(INSRC,"r");
            if(!file)
            {
                printf("can't open file %s\n", INSRC);
                return 0;
            }
            
            // TODO: seems like we don't need separate for loops for IFAULT==1 and IFAULT==0
            if(IFAULT == 1){
                for(i=0;i<NSRC;i++)
                {
                    //TODO: READ_STEP Bug here. "fread(tmpta, sizeof(float), NST*6, file)" reads all NST at once, not just READ_STEP of them
                    if(fread(tmpsrc,sizeof(int),3,file) && fread(tmpta,sizeof(float),NST*6,file))
                    {
                        tpsrc[i*maxdim]   = tmpsrc[0];
                        tpsrc[i*maxdim+1] = tmpsrc[1];
                        tpsrc[i*maxdim+2] = NZ+1-tmpsrc[2];
                        for(j=0;j<READ_STEP;j++)
                        {
                            taxx[i*READ_STEP+j] = tmpta[j*6];
                            tayy[i*READ_STEP+j] = tmpta[j*6+1];
                            tazz[i*READ_STEP+j] = tmpta[j*6+2];
                            taxz[i*READ_STEP+j] = tmpta[j*6+3];
                            tayz[i*READ_STEP+j] = tmpta[j*6+4];
                            taxy[i*READ_STEP+j] = tmpta[j*6+5];
                        }
                    }
                }
                odc::data::Delloc1D(tmpta);
            }
            else if(IFAULT == 0)
                for(i=0;i<NSRC;i++)
                {
                    fscanf(file, " %d %d %d ",&tmpsrc[0], &tmpsrc[1], &tmpsrc[2]);
                    tpsrc[i*maxdim]   = tmpsrc[0];
                    tpsrc[i*maxdim+1] = tmpsrc[1];
                    tpsrc[i*maxdim+2] = NZ+1-tmpsrc[2];
                    //printf("SOURCE: %d,%d,%d\n",tpsrc[0],tpsrc[1],tpsrc[2]);
                    for(j=0;j<READ_STEP;j++){
                        fscanf(file, " %f %f %f %f %f %f ",
                               &taxx[i*READ_STEP+j], &tayy[i*READ_STEP+j],
                               &tazz[i*READ_STEP+j], &taxz[i*READ_STEP+j],
                               &tayz[i*READ_STEP+j], &taxy[i*READ_STEP+j]);
                        //printf("SOURCE VAL %d: %f,%f\n",j,taxx[j],tayy[j]);
                    }
                }
            fclose(file);
            
        } // end if(rank==master)
        
        MPI_Bcast(tpsrc, NSRC*maxdim,    MPI_INT,  master, MCW);
        MPI_Bcast(taxx,  NSRC*READ_STEP, MPI_REAL, master, MCW);
        MPI_Bcast(tayy,  NSRC*READ_STEP, MPI_REAL, master, MCW);
        MPI_Bcast(tazz,  NSRC*READ_STEP, MPI_REAL, master, MCW);
        MPI_Bcast(taxz,  NSRC*READ_STEP, MPI_REAL, master, MCW);
        MPI_Bcast(tayz,  NSRC*READ_STEP, MPI_REAL, master, MCW);
        MPI_Bcast(taxy,  NSRC*READ_STEP, MPI_REAL, master, MCW);
        for(i=0;i<NSRC;i++)
        {
            // Count number of source nodes owned by calling process. If no nodes are owned by calling process then srcproc remains set to -1
            if( tpsrc[i*maxdim]   >= nbx && tpsrc[i*maxdim]   <= nex && tpsrc[i*maxdim+1] >= nby
               && tpsrc[i*maxdim+1] <= ney && tpsrc[i*maxdim+2] >= nbz && tpsrc[i*maxdim+2] <= nez)
            {
                srcproc = rank;
                npsrc ++;
            }
        }
        
        // Copy data for all source nodes owned by calling process into variables with postfix "p" (e.g. "tpsrc" gets copied to "tpsrcp")
        if(npsrc > 0)
        {
            tpsrcp = odc::data::Alloc1P(npsrc*maxdim);
            taxxp  = odc::data::Alloc1D(npsrc*READ_STEP);
            tayyp  = odc::data::Alloc1D(npsrc*READ_STEP);
            tazzp  = odc::data::Alloc1D(npsrc*READ_STEP);
            taxzp  = odc::data::Alloc1D(npsrc*READ_STEP);
            tayzp  = odc::data::Alloc1D(npsrc*READ_STEP);
            taxyp  = odc::data::Alloc1D(npsrc*READ_STEP);
            k      = 0;
            for(i=0;i<NSRC;i++)
            {
                if( tpsrc[i*maxdim]   >= nbx && tpsrc[i*maxdim]   <= nex && tpsrc[i*maxdim+1] >= nby
                   && tpsrc[i*maxdim+1] <= ney && tpsrc[i*maxdim+2] >= nbz && tpsrc[i*maxdim+2] <= nez)
                {
                    tpsrcp[k*maxdim]   = tpsrc[i*maxdim]   - nbx - 1;
                    tpsrcp[k*maxdim+1] = tpsrc[i*maxdim+1] - nby - 1;
                    tpsrcp[k*maxdim+2] = tpsrc[i*maxdim+2] - nbz + 1;
                    for(j=0;j<READ_STEP;j++)
                    {
                        taxxp[k*READ_STEP+j] = taxx[i*READ_STEP+j];
                        tayyp[k*READ_STEP+j] = tayy[i*READ_STEP+j];
                        tazzp[k*READ_STEP+j] = tazz[i*READ_STEP+j];
                        taxzp[k*READ_STEP+j] = taxz[i*READ_STEP+j];
                        tayzp[k*READ_STEP+j] = tayz[i*READ_STEP+j];
                        taxyp[k*READ_STEP+j] = taxy[i*READ_STEP+j];
                    }
                    k++;
                }
            }
        }
        odc::data::Delloc1D(taxx);
        odc::data::Delloc1D(tayy);
        odc::data::Delloc1D(tazz);
        odc::data::Delloc1D(taxz);
        odc::data::Delloc1D(tayz);
        odc::data::Delloc1D(taxy);
        odc::data::Delloc1P(tpsrc);
        
        *SRCPROC = srcproc;
        *NPSRC   = npsrc;
        *ptpsrc  = tpsrcp;
        *ptaxx   = taxxp;
        *ptayy   = tayyp;
        *ptazz   = tazzp;
        *ptaxz   = taxzp;
        *ptayz   = tayzp;
        *ptaxy   = taxyp;
    }
    else if(IFAULT == 2){
        return read_src_ifault_2(rank, READ_STEP,
                                 INSRC, INSRC_I2,
                                 maxdim, coords, NZ,
                                 nxt, nyt, nzt,
                                 NPSRC, SRCPROC,
                                 ptpsrc, ptaxx, ptayy, ptazz,
                                 ptaxz, ptayz, ptaxy, 1);
    }
    return 0;
}

/**
 Perform stress tensor updates at every source fault node owned by the current process
 
 @param i                    Current timestep
 @param DH                   Spatial discretization size
 @param DT                   Timestep length
 @param NST
 @param npsrc                Number of source faults owned by current process
 @param READ_STEP            From function @c command: Number of source fault function timesteps to read at a time
 @param dim                  Number of spatial dimensions (always 3)
 @param psrc                 From function @c inisrc: Array of length <tt>npsrc*dim</tt> that stores indices of nodes owned by calling process that are fault sources \n
 <tt>psrc[i*dim]</tt> = x node index of source fault @c i \n
 <tt>psrc[i*dim+1]</tt> = y node index of source fault @c i \n
 <tt>psrc[i*dim+2]</tt> = z node index of source fault @c i
 @param axx                  From function @c inisrc
 @param ayy                  From function @c inisrc
 @param azz                  From function @c inisrc
 @param axz                  From function @c inisrc
 @param ayz                  From function @c inisrc
 @param axy                  From function @c inisrc
 @param[in,out] xx           Current stress tensor &sigma;_xx value
 @param[in,out] yy           Current stress tensor &sigma;_yy value
 @param[in,out] zz           Current stress tensor &sigma;_zz value
 @param[in,out] xy           Current stress tensor &sigma;_xy value
 @param[in,out] yz           Current stress tensor &sigma;_yz value
 @param[in,out] xz           Current stress tensor &sigma;_xz value
 
 */
void odc::io::Sources::addsrc(int_pt i,      float DH,   float DT,   int NST,  int READ_STEP, int dim,
                              PatchDecomp& pd) {
    
    
    float vtst;
    int_pt idx, idy, idz, j;
    int_pt x, y, z;
    vtst = (float)DT/(DH*DH*DH);
    
    for(j=0; j<m_nPsrc; j++)
    {
        idx = m_ptpSrc[j*dim]-1;
        idy = m_ptpSrc[j*dim+1]-1;
        idz = m_ptpSrc[j*dim+2]-1;

        int patch_id = pd.globalToPatch(idx,idy,idz);
        x = pd.globalToLocalX(idx,idy,idz);
        y = pd.globalToLocalY(idx,idy,idz);
        z = pd.globalToLocalZ(idx,idy,idz);

#ifdef YASK
        Patch& p = pd.m_patches[patch_id];
        double new_xx = p.yask_context.stress_xx->readElem(i,x,y,z, 0) - vtst*m_ptAxx[j*READ_STEP+i];
        double new_xy = p.yask_context.stress_xy->readElem(i,x,y,z, 0) - vtst*m_ptAxy[j*READ_STEP+i];
        double new_xz = p.yask_context.stress_xz->readElem(i,x,y,z, 0) - vtst*m_ptAxz[j*READ_STEP+i];
        double new_yy = p.yask_context.stress_yy->readElem(i,x,y,z, 0) - vtst*m_ptAyy[j*READ_STEP+i];
        double new_yz = p.yask_context.stress_yz->readElem(i,x,y,z, 0) - vtst*m_ptAyz[j*READ_STEP+i];
        double new_zz = p.yask_context.stress_zz->readElem(i,x,y,z, 0) - vtst*m_ptAzz[j*READ_STEP+i];

        p.yask_context.stress_xx->writeElem(new_xx,i,x,y,z, 0);
        p.yask_context.stress_xy->writeElem(new_xy,i,x,y,z, 0);
        p.yask_context.stress_xz->writeElem(new_xz,i,x,y,z, 0);
        p.yask_context.stress_yy->writeElem(new_yy,i,x,y,z, 0);
        p.yask_context.stress_yz->writeElem(new_yz,i,x,y,z, 0);
        p.yask_context.stress_zz->writeElem(new_zz,i,x,y,z, 0);
#else
        pd.m_patches[patch_id].soa.m_stressXX[x][y][z] -= vtst*m_ptAxx[j*READ_STEP+i];
        pd.m_patches[patch_id].soa.m_stressYY[x][y][z] -= vtst*m_ptAyy[j*READ_STEP+i];
        pd.m_patches[patch_id].soa.m_stressZZ[x][y][z] -= vtst*m_ptAzz[j*READ_STEP+i];
        pd.m_patches[patch_id].soa.m_stressXZ[x][y][z] -= vtst*m_ptAxz[j*READ_STEP+i];
        pd.m_patches[patch_id].soa.m_stressYZ[x][y][z] -= vtst*m_ptAyz[j*READ_STEP+i];
        pd.m_patches[patch_id].soa.m_stressXY[x][y][z] -= vtst*m_ptAxy[j*READ_STEP+i];
#endif        
    }
    return;
}
