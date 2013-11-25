#include "DiagnosticScalar.h"

#include "PicParams.h"
#include "DiagParams.h"
#include "SmileiMPI.h"
#include "ElectroMagn.h"

#include <string>

using namespace std;

// constructor
DiagnosticScalar::DiagnosticScalar(PicParams* params, SmileiMPI* smpi) {
	smpi_=smpi;
	if (smpi_->isMaster()) {
		fout.open("scalars.txt");
		if (!fout.is_open()) ERROR("can't open scalar file");
	}
}

DiagnosticScalar::~DiagnosticScalar() {
	if (smpi_->isMaster()) {
		fout.close();
	}
}

// wrapper of the methods
void DiagnosticScalar::run(int timestep, ElectroMagn* EMfields, vector<Species*>& vecSpecies){
	compute_proc_gather(EMfields,vecSpecies);
	compute();
	write(timestep);
}


// it contains all to manage the communication of data. It is "transparent" to the user.
void DiagnosticScalar::compute_proc_gather (ElectroMagn* EMfields, vector<Species*>& vecSpecies) {
	// 	it fills the map on each specie
	for (unsigned int ispec=0; ispec<vecSpecies.size(); ispec++) {
		vecSpecies[ispec]->computeScalar();		
	}
	
	// definition of the memory allocation vector
	vector<double> oneProc;
	for (unsigned int ispec=0; ispec<vecSpecies.size(); ispec++) {
		for (map<string, double>::iterator iter = vecSpecies[ispec]->scalars.begin(); iter != vecSpecies[ispec]->scalars.end(); iter++) {
			oneProc.push_back(iter->second);
        }
	}
	
	// 	it constructs the receiving structure on the master processor	
    vector<double> allProcs;
    if(smpi_->isMaster()){
    	allProcs.resize(smpi_->getSize()*oneProc.size());
    }
	// gathering chars of the master processor
	MPI_Gather(&oneProc[0],oneProc.size(),MPI_DOUBLE,&allProcs[0],oneProc.size(),MPI_DOUBLE,0,MPI_COMM_WORLD);
	
	smpi_->barrier();
	
	// 	method to reconstruct the information on the master processor
	if(smpi_->isMaster()){
		unsigned int count=0;
		mpi_spec_scalars.resize(smpi_->getSize());
		for(int iCPU=0;iCPU<smpi_->getSize();iCPU++){
			mpi_spec_scalars[iCPU].resize(vecSpecies.size());
			for (unsigned int ispec=0; ispec<vecSpecies.size(); ispec++) {
				for (map<string, double>::iterator iter = vecSpecies[ispec]->scalars.begin(); iter != vecSpecies[ispec]->scalars.end(); ++iter) {
					mpi_spec_scalars[iCPU][ispec][iter->first] = allProcs[count];
					count++;
				}
			}			
		}
		if (count!=allProcs.size()) ERROR("problem here " << count << " != " << allProcs.size());
	}
}

// Each scalar diagnostic should be calculated here
void DiagnosticScalar::compute(){	
	if(smpi_->isMaster()){
		out_list.clear();
		for(unsigned int ispec=0; ispec<mpi_spec_scalars[0].size();++ispec){
			double charge_tot=0;
			unsigned int part_tot=0;
			for(int iCPU=0;iCPU<smpi_->getSize();iCPU++){
				charge_tot+=mpi_spec_scalars[iCPU][ispec]["charge_tot"];
 				part_tot+=mpi_spec_scalars[iCPU][ispec]["part_number"];
			}
			if (part_tot) charge_tot/=part_tot;
			
			out_list.push_back(make_pair("charge_tot",charge_tot));
			out_list.push_back(make_pair("part_tot",part_tot));
		}
	}
}

void DiagnosticScalar::write(int itime){	
	if(smpi_->isMaster()){
		if (fout.tellp()==0) {
			fout << "# time";			
			for(vector<pair<string,double> >::iterator iter = out_list.begin(); iter !=out_list.end(); iter++) {
				fout << "\t" << (*iter).first;
			}			
			fout << endl;
		}
		fout << itime;
		for(vector<pair<string,double> >::iterator iter = out_list.begin(); iter !=out_list.end(); iter++) {
			fout << "\t" << (*iter).second;
		}
		fout << endl;
	}
}	
