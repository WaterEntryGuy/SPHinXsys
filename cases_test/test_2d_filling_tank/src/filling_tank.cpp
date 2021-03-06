/**
 * @file 	filling_tank.cpp
 * @brief 	2D exaple to show that a tank is filled by emitter.
 * @details This is the one of the basic test cases, also the first case for
 * 			understanding how emitter inflow is working.
 * @author 	Chi Zhang and Xiangyu Hu
 * @version 0.1
 */
 /**
  * @brief 	SPHinXsys Library.
  */
#include "sphinxsys.h"
  /**
 * @brief Namespace cite here.
 */
using namespace SPH;
/**
 * @brief Basic geometry parameters and numerical setup.
 */
Real DL = 5.366; 						/**< Tank length. */
Real DH = 5.366; 						/**< Tank height. */
Real particle_spacing_ref = 0.025; 		/**< Initial reference particle spacing. */
Real BW = particle_spacing_ref * 4; 	/**< Extending width for wall BCs. */
Real LL = 2.0*BW; 						/**< Inflow region length. */
Real LH = 0.125; 						/**< Inflows region height. */
Real inlet_height = 1.0;				/**< Inflow location height */
Real inlet_distance = - BW;				/**< Inflow location distance */
/**
 * @brief Material properties of the fluid.
 */
Real rho0_f = 1.0;						/**< Reference density of fluid. */
Real gravity_g = 1.0;					/**< Gravity force of fluid. */
Real U_f = 2.0*sqrt(gravity_g* (inlet_height + LH));	/**< Characteristic velocity. */
Real c_f = 10.0*U_f;					/**< Reference sound speed. */

/** @brief create a water block shape for the inlet. */
std::vector<Point> CreatWaterBlockShape()
{
	//geometry
	std::vector<Point> water_block_shape;
	water_block_shape.push_back(Point(inlet_distance, inlet_height));
	water_block_shape.push_back(Point(inlet_distance, LH + inlet_height));
	water_block_shape.push_back(Point(LL + inlet_distance, LH + inlet_height));
	water_block_shape.push_back(Point(LL + inlet_distance, inlet_height));
	water_block_shape.push_back(Point(inlet_distance, inlet_height));

	return water_block_shape;
}

/** @brief 	Fluid body definition. */
class WaterBlock : public FluidBody
{
public:
	WaterBlock(SPHSystem &system, string body_name,
		int refinement_level, ParticlesGeneratorOps op)
		: FluidBody(system, body_name, refinement_level, op)
	{
		/** Geomerty definition. */
		std::vector<Point> water_bock_shape = CreatWaterBlockShape();
		body_region_.add_geometry(new Geometry(water_bock_shape), RegionBooleanOps::add);
		body_region_.done_modeling();

		/** Replace the default kernel functions */
		ReplaceKernelFunction(new KernelTabulated<KernelWendlandC2>(smoothinglength_, 20));
	}
};
/**
 * @brief 	Case dependent material properties definition.
 */
class WaterMaterial : public WeaklyCompressibleFluid
{
public:
	WaterMaterial() : WeaklyCompressibleFluid()
	{
		rho_0_ = rho0_f;
		c_0_ = c_f;

		assignDerivedMaterialParameters();
	}
};
/**
 * @brief 	Wall boundary body definition.
 */
class WallBoundary : public SolidBody
{
public:
	WallBoundary(SPHSystem &system, string body_name,
		int refinement_level, ParticlesGeneratorOps op)
		: SolidBody(system, body_name, refinement_level, op)
	{
		/** Geomerty definition. */
		std::vector<Point> outer_wall_shape;
		outer_wall_shape.push_back(Point(-BW, -BW));
		outer_wall_shape.push_back(Point(-BW, DH + BW));
		outer_wall_shape.push_back(Point(DL + BW, DH + BW));
		outer_wall_shape.push_back(Point(DL + BW, -BW));
		outer_wall_shape.push_back(Point(-BW, -BW));
		Geometry *outer_wall_geometry = new Geometry(outer_wall_shape);
		body_region_.add_geometry(outer_wall_geometry, RegionBooleanOps::add);

		std::vector<Point> inner_wall_shape;
		inner_wall_shape.push_back(Point(0.0, 0.0));
		inner_wall_shape.push_back(Point(0.0, DH));
		inner_wall_shape.push_back(Point(DL, DH));
		inner_wall_shape.push_back(Point(DL, 0.0));
		inner_wall_shape.push_back(Point(0.0, 0.0));
		Geometry *inner_wall_geometry = new Geometry(inner_wall_shape);
		body_region_.add_geometry(inner_wall_geometry, RegionBooleanOps::sub);

		/** Inlet */
		std::vector<Point> water_bock_shape = CreatWaterBlockShape();
		body_region_.add_geometry(new Geometry(water_bock_shape), RegionBooleanOps::sub);
		body_region_.done_modeling();
	}
};

/**
* @brief constrain the beam base
*/
class Inlet : public BodyPartByParticle
{
public:
	Inlet(FluidBody* fluid_body, string constrianed_region_name)
		: BodyPartByParticle(fluid_body, constrianed_region_name)
	{
		/** Geomerty definition. */
		std::vector<Point> water_bock_shape = CreatWaterBlockShape();
		body_part_region_.add_geometry(new Geometry(water_bock_shape), RegionBooleanOps::add);
		body_part_region_.done_modeling();

		/**  Tag the constrained particle. */
		TagBodyPartParticles();
	}
};

/** inlet inflow condition. */
class InletInflowCondition : public fluid_dynamics::EmitterInflowCondition
{
public:
	InletInflowCondition(FluidBody* body, BodyPartByParticle* body_part)
		: EmitterInflowCondition(body, body_part)
	{
		SetInflowParameters();
	}
	
	Vecd GetInflowVelocity(Vecd& position, Vecd& velocity) override {
		return Vec2d(2.0, 0.0);
	}
	void SetInflowParameters() override {
		inflow_pressure_ = 0.0;
	}
};

/**
 * @brief 	Fluid observer body definition.
 */
class FluidObserver : public FictitiousBody
{
public:
	FluidObserver(SPHSystem &system, string body_name, int refinement_level, ParticlesGeneratorOps op)
		: FictitiousBody(system, body_name, refinement_level, 1.3, op)
	{
		body_input_points_volumes_.push_back(make_pair(Point(DL, 0.2), 0.0));
	}
};

/**
 * @brief 	Main program starts here.
 */
int main()
{
	/**
	 * @brief Build up -- a SPHSystem --
	 */
	SPHSystem system(Vec2d(-BW, -BW), Vec2d(DL + BW, DH + BW), particle_spacing_ref);
	/** Set the starting time. */
	GlobalStaticVariables::physical_time_ = 0.0;
	/** Tag for computation from restart files. 0: not from restart files. */
	system.restart_step_ = 0;
	/**
	 * @brief Material property, partilces and body creation of fluid.
	 */
	WaterBlock *water_block 
		= new WaterBlock(system, "WaterBody", 0, ParticlesGeneratorOps::lattice);
	WaterMaterial 	*water_material = new WaterMaterial();
	FluidParticles 	fluid_particles(water_block, water_material);
	/**
	 * @brief 	Particle and body creation of wall boundary.
	 */
	WallBoundary *wall_boundary 
		= new WallBoundary(system, "Wall",	0, ParticlesGeneratorOps::lattice);
	SolidParticles 					solid_particles(wall_boundary);
	/**
	 * @brief 	Particle and body creation of fluid observer.
	 */
	FluidObserver *fluid_observer 
		= new FluidObserver(system, "Fluidobserver", 0, ParticlesGeneratorOps::direct);
	BaseParticles 	observer_particles(fluid_observer);
	/**
	 * @brief 	Body contact map.
	 * @details The contact map gives the data conntections between the bodies.
	 * 			Basically the the rang of bidies to build neighbor particle lists.
	 */
	SPHBodyTopology 	body_topology = { { water_block, { wall_boundary } },
										  { wall_boundary, {} },{ fluid_observer,{ water_block} } };
	system.SetBodyTopology(&body_topology);

	/**
	 * @brief 	Define all numerical methods which are used in this case.
	 */
	 /** Define external force. */
	Gravity 							gravity(Vecd(0.0, -gravity_g));
	 /**
	  * @brief 	Methods used only once.
	  */
	/** Initialize normal direction of the wall boundary. */
	solid_dynamics::NormalDirectionSummation 	get_wall_normal(wall_boundary, {});
	/**
	 * @brief 	Methods used for time stepping.
	 */
	 /** Initialize particle acceleration. */
	InitializeATimeStep 	initialize_a_fluid_step(water_block, &gravity);
	/** Inlet condition. */
	Inlet* inlet = new Inlet(water_block, "Inlet");
	InletInflowCondition inflow_condition(water_block, inlet);
	fluid_dynamics::EmitterInflowInjecting inflow_emitter(water_block, inlet, 300, 0, true);
	/**
	 * @brief 	Algorithms of fluid dynamics.
	 */
	 /** Evaluation of density by summation approach. */
	fluid_dynamics::DensityBySummationFreeSurface 		update_fluid_density(water_block, { wall_boundary });
	/** Time step size without considering sound wave speed. */
	fluid_dynamics::GetAdvectionTimeStepSize 			get_fluid_adevction_time_step_size(water_block, U_f);
	/** Time step size with considering sound wave speed. */
	fluid_dynamics::GetAcousticTimeStepSize get_fluid_time_step_size(water_block);
	/** Pressure relaxation algorithm by using position verlet time stepping. */
	fluid_dynamics::PressureRelaxationFirstHalfRiemann 
		pressure_relaxation_first_half(water_block, { wall_boundary });
	fluid_dynamics::PressureRelaxationSecondHalfRiemann 
		pressure_relaxation_second_half(water_block, { wall_boundary });
	/**
	 * @brief 	Methods used for updating data structure.
	 */
	 /** Update the cell linked list of bodies when neccessary. */
	ParticleDynamicsCellLinkedList			update_cell_linked_list(water_block);
	/** Update the configuration of bodies when neccessary. */
	ParticleDynamicsConfiguration 			update_particle_configuration(water_block);
	/** Update the interact configuration of bodies when neccessary. */
	ParticleDynamicsInteractionConfiguration 	
		update_observer_interact_configuration(fluid_observer, { water_block });
	/**
	 * @brief Output.
	 */
	In_Output in_output(system);
	/** Output the body states. */
	WriteBodyStatesToVtu 		write_body_states(in_output, system.real_bodies_);
	/** Output the body states for restart simulation. */
	ReadRestart		read_restart_files(in_output, system.real_bodies_);
	WriteRestart	write_restart_files(in_output, system.real_bodies_);
	/** Output the mechanical energy of fluid body. */
	WriteTotalMechanicalEnergy 	write_water_mechanical_energy(in_output, water_block, &gravity);
	/** output the observed data from fluid body. */
	WriteAnObservedQuantity<Real, FluidParticles,
		FluidParticleData, &FluidParticles::fluid_particle_data_, &FluidParticleData::p_>
		write_recorded_water_pressure("Pressure", in_output, fluid_observer, water_block);
	
	/**
	 * @brief Setup goematrics and initial conditions.
	 */
	system.InitializeSystemCellLinkedLists();
	system.InitializeSystemConfigurations();
	get_wall_normal.exec();
	/**
	 * @brief The time stepping starts here.
	 */
	 /** If the starting time is not zero, please setup the restart time step ro read in restart states. */
	if (system.restart_step_ != 0)
	{
		GlobalStaticVariables::physical_time_ = read_restart_files.ReadRestartFiles(system.restart_step_);
		update_cell_linked_list.parallel_exec();
		update_particle_configuration.parallel_exec();
	}
	/** Output the start states of bodies. */
	write_body_states.WriteToFile(GlobalStaticVariables::physical_time_);
	/** Output the Hydrostatic mechanical energy of fluid. */
	write_water_mechanical_energy.WriteToFile(GlobalStaticVariables::physical_time_);
	/**
	 * @brief 	Basic parameters.
	 */
	int number_of_iterations = system.restart_step_;
	int screen_output_interval = 100;
	int restart_output_interval = screen_output_interval*10;
	Real End_Time = 50.0; 	/**< End time. */
	Real D_Time = 0.1;		/**< Time stamps for output of body states. */
	Real Dt = 0.0;			/**< Default advection time step sizes. */
	Real dt = 0.0; 			/**< Default accoustic time step sizes. */
	/** statistics for computing CPU time. */
	tick_count t1 = tick_count::now();
	tick_count::interval_t interval;

	/**
	 * @brief 	Main loop starts here.
	 */
	while (GlobalStaticVariables::physical_time_ < End_Time)
	{
		Real integeral_time = 0.0;
		/** Integrate time (loop) until the next output time. */
		while (integeral_time < D_Time)
		{
			/** Acceleration due to viscous force and gravity. */
			initialize_a_fluid_step.parallel_exec();
			Dt = get_fluid_adevction_time_step_size.parallel_exec();
			update_fluid_density.parallel_exec();

			/** Dynamics including pressure relaxation. */
			Real relaxation_time = 0.0;
			while (relaxation_time < Dt)
			{
				pressure_relaxation_first_half.parallel_exec(dt);
				inflow_condition.parallel_exec();
				pressure_relaxation_second_half.parallel_exec(dt);
				dt = get_fluid_time_step_size.parallel_exec();
				relaxation_time += dt;
				integeral_time += dt;
				GlobalStaticVariables::physical_time_ += dt;
			}

			if (number_of_iterations % screen_output_interval == 0)
			{
				cout << fixed << setprecision(9) << "N=" << number_of_iterations << "	Time = "
					<< GlobalStaticVariables::physical_time_
					<< "	Dt = " << Dt << "	dt = " << dt << "\n";

				if (number_of_iterations % restart_output_interval == 0)
					write_restart_files.WriteToFile(Real(number_of_iterations));
			}
			number_of_iterations++;

			/** impose inflow condition*/
			inflow_emitter.exec();

			/** Update cell linked list and configuration. */
			update_cell_linked_list.parallel_exec();
			update_particle_configuration.parallel_exec();
			update_observer_interact_configuration.parallel_exec();
		}

		tick_count t2 = tick_count::now();
		write_water_mechanical_energy.WriteToFile(GlobalStaticVariables::physical_time_);
		write_body_states.WriteToFile(GlobalStaticVariables::physical_time_);
		write_recorded_water_pressure.WriteToFile(GlobalStaticVariables::physical_time_);
		tick_count t3 = tick_count::now();
		interval += t3 - t2;

	}
	tick_count t4 = tick_count::now();

	tick_count::interval_t tt;
	tt = t4 - t1 - interval;
	cout << "Total wall time for computation: " << tt.seconds()
		<< " seconds." << endl;

	return 0;
}
