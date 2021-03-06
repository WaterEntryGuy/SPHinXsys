/**
 * @file 	depolarization.cpp
 * @brief 	This is the first test to validate our PED-ODE solver for solving
 * 			elctrophysiology monodomain model closed by a physiology reaction.
 * @author 	Chi Zhang and Xiangyu Hu
 * @version 0.2.1
 * 			From here, I will denote version a beta, e.g. 0.2.1, other than 0.1 as
 * 			we will introduce cardiac electrophysiology and cardaic mechanics herein.
 * 			Chi Zhang
 */

/** SPHinXsys Library. */
#include "sphinxsys.h"
/** Namespace cite here. */
using namespace SPH;
/** Geometry parameter. */
Real L = 1.0; 	
Real H = 1.0;
/** Reference particle spacing. */
Real particle_spacing_ref = H / 50.0;
/** Electrophysiology prameters. */
Real diffusion_coff_ = 1.0;
Real bias_diffusion_coff_ = 0.0;
Vec2d fiber_direction(1.0, 0.0);
Real c_m = 1.0;
Real k = 8.0;
Real a = 0.15;
Real mu_1 = 0.2;
Real mu_2 = 0.3;
Real epsilon = 0.04;
Real k_a = 0.0;
std::vector<Point> CreatShape()
{
	std::vector<Point> shape;
	shape.push_back(Point(0.0, 0.0));
	shape.push_back(Point(0.0,  H));
	shape.push_back(Point(L ,  H));
	shape.push_back(Point(L, 0.0));
	shape.push_back(Point(0.0, 0.0));
	return shape;
}
/** 
 * Define geometry and initial conditions of SPH bodies. 
 * */
class MuscleBody : public SolidBody
{
public:
	MuscleBody(SPHSystem &system, string body_name, int refinement_level, ParticlesGeneratorOps op)
		: SolidBody(system, body_name, refinement_level, op)
	{
		std::vector<Point> body_shape = CreatShape();
		body_region_.add_geometry(new Geometry(body_shape), RegionBooleanOps::add);
		body_region_.done_modeling();
	}
};
/**
 * Voltage observer body definition.
 */
class VoltageObserver : public FictitiousBody
{
public:
	VoltageObserver(SPHSystem &system, string body_name, int refinement_level, ParticlesGeneratorOps op)
		: FictitiousBody(system, body_name, refinement_level, 1.3, op)
	{
		/** postion and volume. */
		body_input_points_volumes_.push_back(make_pair(Point(0.3, 0.7), 0.0));
	}
};

/**
 * Setup eletro_physiology reation properties
 */
class MuscleReactionModel : public AlievPanfilowModel
{
public:
	MuscleReactionModel() : AlievPanfilowModel()
	{
		/** Basic reaction parameters*/
		k_a_ = k_a;
		c_m_ = c_m;
		k_ = k;
		a_ = a;
		mu_1_ = mu_1;
		mu_2_ = mu_2;
		epsilon_ = epsilon;

		/** Compute the derived material parameters*/
		assignDerivedReactionParameters();
	}
};

/**
 * Setup material properties of myocardium
 */
class MyocardiumMuscle
 	: public MonoFieldElectroPhysiology
{
 public:
 	MyocardiumMuscle(ElectroPhysiologyReaction* electro_physiology_reaction)
		: MonoFieldElectroPhysiology(electro_physiology_reaction)
	{
		/** Basic material parameters*/
		diff_cf_ = diffusion_coff_;
		bias_diff_cf_ = bias_diffusion_coff_;
		bias_direction_ = fiber_direction;

		/** Compute the derived material parameters. */
		assignDerivedMaterialParameters();
		/** Create the vector of diffusion properties. */
		initializeDiffusion();
	}
};
 /**
 * application dependent initial condition 
 */
class DepolarizationInitialCondition
	: public electro_physiology::ElectroPhysiologyInitialCondition
{
protected:
	size_t voltage_;

	void Update(size_t index_particle_i, Real dt) override
	{
		BaseParticleData &base_particle_data_i = particles_->base_particle_data_[index_particle_i];
		DiffusionReactionData& electro_physiology_data_i = particles_->diffusion_reaction_data_[index_particle_i];

		electro_physiology_data_i.species_n_[voltage_] = exp(-4.0 * ((base_particle_data_i.pos_n_[0] - 1.0)
				* (base_particle_data_i.pos_n_[0] - 1.0) + base_particle_data_i.pos_n_[1] * 
				base_particle_data_i.pos_n_[1]));
	};
public:
	DepolarizationInitialCondition(SolidBody* muscle)
		: electro_physiology::ElectroPhysiologyInitialCondition(muscle) {
		voltage_ = material_->getSpeciesIndexMap()["Voltage"];
	};
};

/** 
 * The main program. 
 */
int main()
{
	/** 
	 * Build up context -- a SPHSystem. 
	 */
	SPHSystem system(Vec2d(0.0, 0.0), Vec2d(L, H), particle_spacing_ref);
		GlobalStaticVariables::physical_time_ = 0.0;
	/** 
	 * Configuration of materials, crate particle container and muscle body. 
	 */
	MuscleBody *muscle_body  =  new MuscleBody(system, "MuscleBody", 0, ParticlesGeneratorOps::lattice);
	MuscleReactionModel *muscle_reaction_model = new MuscleReactionModel();
	MyocardiumMuscle 	*myocardium_musscle = new MyocardiumMuscle(muscle_reaction_model);
	ElectroPhysiologyParticles 		pmyocardium_musscle_articles(muscle_body, myocardium_musscle);
	/**
	 * Particle and body creation of fluid observer.
	 */
	VoltageObserver *voltage_observer = new VoltageObserver(system, "VoltageObserver", 0, ParticlesGeneratorOps::direct);
	BaseParticles 					observer_particles(voltage_observer);
	/** 
	 * Set body contact map. 
	 */
	SPHBodyTopology body_topology = { { muscle_body, {  } }, {voltage_observer, {muscle_body}} };
	system.SetBodyTopology(&body_topology);

	/**
	 * The main dynamics algorithm is defined start here.
	 */
	/**
	 * Initialization for electrophysiology computation. 
	 */	
	DepolarizationInitialCondition 								initialization(muscle_body);
	/** 
	 * Corrected strong configuration. 
	 */	
	solid_dynamics::CorrectConfiguration 					correct_configuration(muscle_body);
	/** 
	 * Time step size caclutation. 
	 */
	electro_physiology::GetElectroPhysiologyTimeStepSize 		get_time_step_size(muscle_body);
	/** 
	 * Diffusion process for diffusion body. 
	 */
	electro_physiology::ElectroPhysiologyDiffusionRelaxation 		diffusion_relaxation(muscle_body);
	/** 
	 * Sovlers for ODE system 
	 */
	electro_physiology::ElectroPhysiologyReactionRelaxationForward 		reaction_relaxation_forward(muscle_body);
	electro_physiology::ElectroPhysiologyReactionRelaxationBackward 	reaction_relaxation_backward(muscle_body);
	/** Outputs. */
	/**
	 * Simple input and outputs.
	 */
	In_Output 							in_output(system);
	WriteBodyStatesToVtu 				write_states(in_output, system.real_bodies_);
	WriteObservedDiffusionReactionQuantity<ElectroPhysiologyParticles>
		write_recorded_voltage("Voltage", in_output, voltage_observer, muscle_body);

	/** 
	 * Pre-simultion. 
	 */
	system.InitializeSystemCellLinkedLists();
	system.InitializeSystemConfigurations();
	initialization.exec();
	correct_configuration.parallel_exec();
	/** 
	 * Output global basic parameters. 
	 */
	write_states.WriteToFile(GlobalStaticVariables::physical_time_);
	write_recorded_voltage.WriteToFile(GlobalStaticVariables::physical_time_);

	int ite 		= 0;
	Real T0 		= 16.0;
	Real End_Time 	= T0;
	Real D_Time 	= 0.5; 				/**< Time period for output */
	Real Dt 		= 0.01 * D_Time;	/**< Time period for data observing */
	Real dt		 	= 0.0;
	/** Statistics for computing time. */
	tick_count t1 = tick_count::now();
	tick_count::interval_t interval;
	/** Main loop starts here. */ 
	while (GlobalStaticVariables::physical_time_ < End_Time)
	{
		Real integeral_time = 0.0;
		while (integeral_time < D_Time) 
		{
			Real relaxation_time = 0.0;
			while (relaxation_time < Dt) 
			{
				if (ite % 1000 == 0) 
				{
					cout << "N=" << ite << " Time: "
						<< GlobalStaticVariables::physical_time_ << "	dt: "
						<< dt << "\n";
				}
				/**Strang's splitting method. */
				reaction_relaxation_forward.parallel_exec(0.5 * dt);
				diffusion_relaxation.parallel_exec(dt);
				reaction_relaxation_backward.parallel_exec(0.5 * dt);

				ite++;
				dt = get_time_step_size.parallel_exec();
				relaxation_time += dt;
				integeral_time += dt;
				GlobalStaticVariables::physical_time_ += dt;
			}
			write_recorded_voltage.WriteToFile(GlobalStaticVariables::physical_time_);
		}

		tick_count t2 = tick_count::now();
		write_states.WriteToFile(GlobalStaticVariables::physical_time_);
		tick_count t3 = tick_count::now();
		interval += t3 - t2;
	}
	tick_count t4 = tick_count::now();

	tick_count::interval_t tt;
	tt = t4 - t1 - interval;
	cout << "Total wall time for computation: " << tt.seconds() << " seconds." << endl;

	return 0;
}
