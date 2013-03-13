// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.hpp"

// ==========================================================================
// Enemy
// ==========================================================================

namespace { // UNNAMED NAMESPACE

// Enemy actions are generic to serve both enemy_t and enemy_add_t,
// so they can only rely on player_t and should have no knowledge of class definitions

// Melee ====================================================================

struct melee_t : public melee_attack_t
{
  melee_t( const std::string& name, player_t* player ) :
    melee_attack_t( name, player, spell_data_t::nil() )
  {
    school = SCHOOL_PHYSICAL;
    may_crit    = true;
    background  = true;
    repeating   = true;
    trigger_gcd = timespan_t::zero();
    base_dd_min = 260000;
    base_execute_time = timespan_t::from_seconds( 2.4 );
    aoe = -1;
  }

  virtual size_t available_targets( std::vector< player_t* >& tl )
  {
    // TODO: This does not work for heals at all, as it presumes enemies in the
    // actor list.
    tl.clear();
    tl.push_back( target );

    for ( size_t i = 0, actors = sim -> actor_list.size(); i < actors; i++ )
    {
      //only add non heal_target tanks to this list for now
      if ( !sim -> actor_list[ i ] -> is_sleeping() &&
           !sim -> actor_list[ i ] -> is_enemy() &&
           sim -> actor_list[ i ] -> primary_role() == ROLE_TANK &&
           sim -> actor_list[ i ] != target &&
           sim -> actor_list[ i ] != sim -> heal_target )
        tl.push_back( sim -> actor_list[ i ] );
    }
    //if we have no target (no tank), add the healing target as substitute
    if ( tl.empty() )
    {
      tl.push_back( sim->heal_target );
    }


    return tl.size();
  }
};

// Auto Attack ==============================================================

struct auto_attack_t : public attack_t
{
  auto_attack_t( player_t* p, const std::string& options_str ) :
    attack_t( "auto_attack", p, spell_data_t::nil() )
  {
    school = SCHOOL_PHYSICAL;
    p -> main_hand_attack = new melee_t( "melee_main_hand", p );
    p -> main_hand_attack -> weapon = &( p -> main_hand_weapon );
    p -> main_hand_attack -> base_execute_time = timespan_t::from_seconds( 2.4 );

    int aoe_tanks = 0;
    option_t options[] =
    {
      opt_float( "damage", p -> main_hand_attack -> base_dd_min ),
      opt_timespan( "attack_speed", p -> main_hand_attack -> base_execute_time ),
      opt_bool( "aoe_tanks", aoe_tanks ),
      opt_null()
    };
    parse_options( options, options_str );

    p -> main_hand_attack -> target = target;

    if ( aoe_tanks == 1 )
      p -> main_hand_attack -> aoe = -1;
    else
      p->main_hand_attack->aoe=aoe_tanks;

    p -> main_hand_attack -> base_dd_max = p -> main_hand_attack -> base_dd_min;
    if ( p -> main_hand_attack -> base_execute_time < timespan_t::from_seconds( 0.01 ) )
      p -> main_hand_attack -> base_execute_time = timespan_t::from_seconds( 2.4 );

    cooldown = player -> get_cooldown( name_str + "_" + target -> name() );
    stats = player -> get_stats( name_str + "_" + target -> name(), this );
    name_str = name_str + "_" + target -> name();

    trigger_gcd = timespan_t::zero();
  }

  virtual void execute()
  {
    player -> main_hand_attack -> schedule_execute();
    if ( player -> off_hand_attack )
    {
      player -> off_hand_attack -> schedule_execute();
    }
  }

  virtual bool ready()
  {
    if ( player -> is_moving() ) return false;
    return( player -> main_hand_attack -> execute_event == 0 ); // not swinging
  }
};

// Spell Nuke ===============================================================

struct spell_nuke_t : public spell_t
{
  spell_nuke_t( player_t* p, const std::string& options_str ) :
    spell_t( "spell_nuke", p, spell_data_t::nil() )
  {
    school = SCHOOL_FIRE;
    base_execute_time = timespan_t::from_seconds( 3.0 );
    base_dd_min = 50000;

    cooldown = player -> get_cooldown( name_str + "_" + target -> name() );

    int aoe_tanks = 0;
    option_t options[] =
    {
      opt_float( "damage", base_dd_min ),
      opt_timespan( "attack_speed", base_execute_time ),
      opt_timespan( "cooldown",     cooldown -> duration ),
      opt_bool( "aoe_tanks", aoe_tanks ),
      opt_null()
    };
    parse_options( options, options_str );

    base_dd_max = base_dd_min;
    if ( base_execute_time < timespan_t::zero() )
      base_execute_time = timespan_t::from_seconds( 3.0 );

    stats = player -> get_stats( name_str + "_" + target -> name(), this );
    name_str = name_str + "_" + target -> name();

    may_crit = false;
    if ( aoe_tanks == 1 )
      aoe = -1;
  }

  virtual size_t available_targets( std::vector< player_t* >& tl )
  {
    // TODO: This does not work for heals at all, as it presumes enemies in the
    // actor list.

    tl.clear();
    tl.push_back( target );
    for ( size_t i = 0, actors = sim -> actor_list.size(); i < actors; i++ )
    {
      //only add non heal_target tanks to this list for now
      if ( !sim -> actor_list[ i ] -> is_sleeping() &&
           !sim -> actor_list[ i ] -> is_enemy() &&
           sim -> actor_list[ i ] -> primary_role() == ROLE_TANK &&
           sim -> actor_list[ i ] != target &&
           sim -> actor_list[ i ] != sim -> heal_target )
        tl.push_back( sim -> actor_list[ i ] );
    }
    //if we have no target (no tank), add the healing target as substitute
    if ( tl.empty() )
    {
      tl.push_back( sim->heal_target );
    }
    return tl.size();
  }

};

// Spell AoE ================================================================

struct spell_aoe_t : public spell_t
{
  spell_aoe_t( player_t* p, const std::string& options_str ) :
    spell_t( "spell_aoe", p, spell_data_t::nil() )
  {
    school = SCHOOL_FIRE;
    base_execute_time = timespan_t::from_seconds( 3.0 );
    base_dd_min = 50000;

    cooldown = player -> get_cooldown( name_str + "_" + target -> name() );

    option_t options[] =
    {
      opt_float( "damage", base_dd_min ),
      opt_timespan( "cast_time", base_execute_time ),
      opt_timespan( "cooldown", cooldown -> duration ),
      opt_null()
    };
    parse_options( options, options_str );

    base_dd_max = base_dd_min;
    if ( base_execute_time < timespan_t::from_seconds( 0.01 ) )
      base_execute_time = timespan_t::from_seconds( 3.0 );

    stats = player -> get_stats( name_str + "_" + target -> name(), this );
    name_str = name_str + "_" + target -> name();

    may_crit = false;

    aoe = -1;
  }

  virtual size_t available_targets( std::vector< player_t* >& tl )
  {
    // TODO: This does not work for heals at all, as it presumes enemies in the
    // actor list.

    tl.clear();
    tl.push_back( target );

    for ( size_t i = 0, actors = sim -> actor_list.size(); i < actors; ++i )
    {
      if ( ! sim -> actor_list[ i ] -> is_sleeping() &&
           !sim -> actor_list[ i ] -> is_enemy() &&
           sim -> actor_list[ i ] != target )
        tl.push_back( sim -> actor_list[ i ] );
    }

    return tl.size();
  }

};

// Summon Add ===============================================================

struct summon_add_t : public spell_t
{
  std::string add_name;
  timespan_t summoning_duration;
  pet_t* pet;

  summon_add_t( player_t* p, const std::string& options_str ) :
    spell_t( "summon_add", player, spell_data_t::nil() ),
    add_name( "" ), summoning_duration( timespan_t::zero() ), pet( 0 )
  {
    option_t options[] =
    {
      opt_string( "name", add_name ),
      opt_timespan( "duration", summoning_duration ),
      opt_timespan( "cooldown", cooldown -> duration ),
      opt_null()
    };
    parse_options( options, options_str );

    school = SCHOOL_PHYSICAL;
    pet = p -> find_pet( add_name );
    if ( ! pet )
    {
      sim -> errorf( "Player %s unable to find pet %s for summons.\n", p -> name(), add_name.c_str() );
      sim -> cancel();
    }

    harmful = false;

    trigger_gcd = timespan_t::from_seconds( 1.5 );
  }

  virtual void execute()
  {
    spell_t::execute();

    player -> summon_pet( add_name, summoning_duration );
  }

  virtual bool ready()
  {
    if ( ! pet -> is_sleeping() )
      return false;

    return spell_t::ready();
  }
};

static action_t* enemy_create_action( player_t* p, const std::string& name, const std::string& options_str )
{
  if ( name == "auto_attack" ) return new auto_attack_t( p, options_str );
  if ( name == "spell_nuke"  ) return new  spell_nuke_t( p, options_str );
  if ( name == "spell_aoe"   ) return new   spell_aoe_t( p, options_str );
  if ( name == "summon_add"  ) return new  summon_add_t( p, options_str );

  return NULL;
}

struct enemy_t : public player_t
{
  double fixed_health, initial_health;
  double fixed_health_percentage, initial_health_percentage;
  timespan_t waiting_time;

  std::vector<buff_t*> buffs_health_decades;

  enemy_t( sim_t* s, const std::string& n, race_e r = RACE_HUMANOID, player_e type = ENEMY ) :
    player_t( s, type, n, r ),
    fixed_health( 0 ), initial_health( 0 ),
    fixed_health_percentage( 0 ), initial_health_percentage( 100.0 ),
    waiting_time( timespan_t::from_seconds( 1.0 ) )
  {
    s -> target_list.push_back( this );
    position_str = "front";
    level = level + 3;
  }

  virtual role_e primary_role()
  { return ROLE_TANK; }

  virtual resource_e primary_resource()
  { return RESOURCE_MANA; }

  virtual action_t* create_action( const std::string& name, const std::string& options_str );
  virtual void init_base();
  virtual void create_buffs();
  virtual void init_resources( bool force=false );
  virtual void init_target();
  virtual void init_actions();
  virtual double composite_tank_block();
  virtual double resource_loss( resource_e, double, gain_t*, action_t* );
  virtual void create_options();
  virtual pet_t* create_pet( const std::string& add_name, const std::string& pet_type = std::string() );
  virtual void create_pets();
  virtual double health_percentage();
  virtual void combat_begin();
  virtual void combat_end();
  virtual void recalculate_health();
  virtual expr_t* create_expression( action_t* action, const std::string& type );
  virtual timespan_t available() { return waiting_time; }
};

// ==========================================================================
// Enemy Add
// ==========================================================================

struct add_t : public pet_t
{
  add_t( sim_t* s, enemy_t* o, const std::string& n, pet_e pt = PET_ENEMY ) :
    pet_t( s, o, n, pt )
  {
    level = default_level + 3;
  }

  virtual void init_actions()
  {
    if ( action_list_str.empty() )
    {
      action_list_str += "/snapshot_stats";
    }

    pet_t::init_actions();
  }

  virtual resource_e primary_resource()
  { return RESOURCE_HEALTH; }

  virtual action_t* create_action( const std::string& name,
                                   const std::string& options_str )
  {
    action_t* a = enemy_create_action( this, name, options_str );

    if ( !a )
      a = pet_t::create_action( name, options_str );

    return a;
  }
};

struct heal_enemy_t : public enemy_t
{
  heal_enemy_t( sim_t* s, const std::string& n, race_e r = RACE_HUMANOID ) :
    enemy_t( s, n, r, HEALING_ENEMY )
  {
    target = this;
    level = default_level; // set to default player level, not default+3
  }

  virtual void init_resources( bool /* force */ )
  {
    resources.base[ RESOURCE_HEALTH ] = 100000000.0;

    player_t::init_resources( true );

    resources.current[ RESOURCE_HEALTH ] = resources.base[ RESOURCE_HEALTH ] / 10;
  }
  virtual void init_base()
  {
    enemy_t::init_base();

    htps.change_mode( false );

    level = std::min( 90, level );
  }
  virtual resource_e primary_resource()
  { return RESOURCE_HEALTH; }

};

// enemy_t::create_action ===================================================

action_t* enemy_t::create_action( const std::string& name,
                                  const std::string& options_str )
{
  action_t* a = enemy_create_action( this, name, options_str );

  if ( !a )
    a = player_t::create_action( name, options_str );

  return a;
}

// enemy_t::init_base =======================================================

void enemy_t::init_base()
{
  level = sim -> max_player_level + 3;

  if ( sim -> target_level >= 0 )
    level = sim -> target_level;
  else if ( ( sim -> max_player_level + sim -> rel_target_level ) >= 0 )
    level = sim -> max_player_level + sim -> rel_target_level;

  waiting_time = timespan_t::from_seconds( std::min( ( int ) floor( sim -> max_time.total_seconds() ), sim -> wheel_seconds - 1 ) );
  if ( waiting_time < timespan_t::from_seconds( 1.0 ) )
    waiting_time = timespan_t::from_seconds( 1.0 );

  base.attack_crit = 0.05;

  if ( initial.armor <= 0 )
  {
    double& a = initial.armor;

    switch ( level )
    {
    case 80: a = 9616; break;
    case 81: a = 10034; break;
    case 82: a = 10338; break;
    case 83: a = 10643; break;
    case 84: a = 10880; break;
    case 85: a = 11092; break;
    case 86: a = 11387; break;
    case 87: a = 11682; break;
    case 88: a = 11977; break;
    case 89: a = 17960; break;
    case 90: a = 19680; break;
    case 91: a = 21400; break; // TO-DO: Confirm.
    case 92: a = 23115; break; // TO-DO: Confirm.
    case 93: a = 24835; break;
    default: if ( level < 80 )
        a = ( int ) floor ( ( level / 80.0 ) * 9729 ); // Need a better value here.
      break;
    }
  }
  base.armor = initial.armor;

  initial_health = ( sim -> overrides.target_health ) ? sim -> overrides.target_health : fixed_health;

  if ( ( initial_health_percentage < 1   ) ||
       ( initial_health_percentage > 100 ) )
  {
    initial_health_percentage = 100.0;
  }
}

// enemy_t::init_buffs ==================================================

void enemy_t::create_buffs()
{
  player_t::create_buffs();

  for ( unsigned int i = 1; i <= 10; ++ i )
    buffs_health_decades.push_back( buff_creator_t( this, "Health Decade (" + util::to_string( ( i - 1 )* 10 ) + " - " + util::to_string( i * 10 ) + ")" ) );
}

// enemy_t::init_resources ==================================================

void enemy_t::init_resources( bool /* force */ )
{
  double health_adjust = 1.0 + sim -> vary_combat_length * sim -> iteration_adjust();

  resources.base[ RESOURCE_HEALTH ] = initial_health * health_adjust;

  player_t::init_resources( true );

  if ( initial_health_percentage > 0 )
  {
    resources.max[ RESOURCE_HEALTH ] *= 100.0 / initial_health_percentage;
  }
}

// enemy_t::init_target ====================================================

void enemy_t::init_target()
{
  if ( ! target_str.empty() )
  {
    target = sim -> find_player( target_str );
  }

  if ( target )
    return;

  for ( size_t i = 0; i < sim -> player_list.size(); ++i )
  {
    player_t* q = sim -> player_list[ i ];
    if ( q -> primary_role() != ROLE_TANK )
      continue;

    target = q;
    break;
  }

  if ( !target )
    target = sim -> target;
}

// enemy_t::init_actions ====================================================

void enemy_t::init_actions()
{
  if ( ! is_add() && is_enemy() )
  {
    if ( action_list_str.empty() )
    {
      std::string& precombat_list = get_action_priority_list( "precombat" ) -> action_list_str;
      precombat_list += "/snapshot_stats";

      if ( ! target -> is_enemy() )
      {
        action_list_str += "/auto_attack,damage=700000,attack_speed=2,aoe_tanks=1";
        action_list_str += "/spell_nuke,damage=60000,cooldown=4,attack_speed=2,aoe_tanks=1";
      }
      else if ( sim -> heal_target && this != sim -> heal_target )
      {
        unsigned int healers = 0;
        for ( size_t i = 0; i < sim -> player_list.size(); ++i )
          if ( !sim -> player_list[ i ] -> is_pet() && sim -> player_list[ i ] -> primary_role() == ROLE_HEAL )
            ++healers;

        action_list_str += "/auto_attack,damage=" + util::to_string( 200000 * healers * level / 85 ) + ",attack_speed=2.0,target=" + sim -> heal_target -> name_str;
      }
    }
  }
  player_t::init_actions();

  // Small hack to increase waiting time for target without any actions
  for ( size_t i = 0; i < action_list.size(); ++i )
  {
    action_t* action = action_list[ i ];
    if ( action -> background ) continue;
    if ( action -> name_str == "snapshot_stats" ) continue;
    if ( action -> name_str.find( "auto_attack" ) != std::string::npos )
      continue;
    waiting_time = timespan_t::from_seconds( 1.0 );
    break;
  }
}

// enemy_t::composite_tank_block ============================================

double enemy_t::composite_tank_block()
{
  double b = player_t::composite_tank_block();

  b += 0.05;

  return b;
}

// enemy_t::resource_loss ==================================================

double enemy_t::resource_loss( resource_e resource_type,
                               double    amount,
                               gain_t*   source,
                               action_t* action )
{
  // This mechanic compares pre and post health decade, and if it switches to a lower decade,
  // it triggers the respective trigger buff.
  // Starting decade ( 100% to 90% ) is triggered in combat_begin()

  int pre_health = static_cast<int>( resources.pct( RESOURCE_HEALTH ) * 100 ) / 10;
  double r = player_t::resource_loss( resource_type, amount, source, action );
  int post_health = static_cast<int>( resources.pct( RESOURCE_HEALTH ) * 100 ) / 10;

  if ( pre_health < 10 && pre_health > post_health && post_health >= 0 )
  {
    if ( static_cast<unsigned int>( post_health + 1 ) < buffs_health_decades.size() )
      buffs_health_decades.at( post_health + 1 ) -> expire();
    buffs_health_decades.at( post_health ) -> trigger();
  }

  return r;
}

// enemy_t::create_options ==================================================

void enemy_t::create_options()
{
  option_t target_options[] =
  {
    opt_float( "target_health", fixed_health ),
    opt_float( "target_initial_health_percentage", initial_health_percentage ),
    opt_float( "target_fixed_health_percentage", fixed_health_percentage ),
    opt_string( "target_tank", target_str ),
    opt_null()
  };

  option_t::copy( sim -> options, target_options );

  player_t::create_options();
}

// enemy_t::create_add ======================================================

pet_t* enemy_t::create_pet( const std::string& add_name, const std::string& /* pet_type */ )
{
  pet_t* p = find_pet( add_name );
  if ( !p )
    p = new add_t( sim, this, add_name, PET_ENEMY );
  return p;
}

// enemy_t::create_pets =====================================================

void enemy_t::create_pets()
{
  for ( int i=0; i < sim -> target_adds; i++ )
  {
    create_pet( "add" + util::to_string( i ) );
  }
}

// enemy_t::health_percentage() =============================================

double enemy_t::health_percentage()
{
  if ( fixed_health_percentage > 0 ) return fixed_health_percentage;

  if ( resources.base[ RESOURCE_HEALTH ] == 0 ) // first iteration
  {
    timespan_t remainder = std::max( timespan_t::zero(), ( sim -> expected_time - sim -> current_time ) );

    return ( remainder / sim -> expected_time ) * ( initial_health_percentage - sim -> target_death_pct ) + sim ->  target_death_pct;
  }

  return resources.pct( RESOURCE_HEALTH ) * 100 ;
}

// enemy_t::recalculate_health ==============================================

void enemy_t::recalculate_health()
{
  if ( sim -> expected_time <= timespan_t::zero() || fixed_health > 0 ) return;

  if ( initial_health == 0 ) // first iteration
  {
    initial_health = iteration_dmg_taken * ( sim -> expected_time / sim -> current_time ) * ( 1.0 / ( 1.0 - sim ->  target_death_pct / 100 ) );
  }
  else
  {
    timespan_t delta_time = sim -> current_time - sim -> expected_time;
    delta_time /= ( sim -> current_iteration + 1 ); // dampening factor
    double factor = 1.0 - ( delta_time / sim -> expected_time );

    if ( factor > 1.5 ) factor = 1.5;
    if ( factor < 0.5 ) factor = 0.5;

    initial_health *= factor;
  }

  if ( sim -> debug ) sim -> output( "Target %s initial health calculated to be %.0f. Damage was %.0f", name(), initial_health, iteration_dmg_taken );
}

// enemy_t::create_expression ===============================================

expr_t* enemy_t::create_expression( action_t* action,
                                    const std::string& name_str )
{
  if ( name_str == "adds" )
    return make_ref_expr( name_str, active_pets );

  // override enemy health.pct expression
  if ( name_str == "health.pct" )
    return make_mem_fn_expr( name_str, *this, &enemy_t::health_percentage );

  return player_t::create_expression( action, name_str );
}

// enemy_t::combat_begin ======================================================

void enemy_t::combat_begin()
{
  player_t::combat_begin();

  buffs_health_decades[ 9 ] -> trigger();
}

// enemy_t::combat_end ======================================================

void enemy_t::combat_end()
{
  player_t::combat_end();

  if ( ! sim -> overrides.target_health )
    recalculate_health();
}

// ENEMY MODULE INTERFACE ================================================

struct enemy_module_t : public module_t
{
  enemy_module_t() : module_t( ENEMY ) {}

  virtual player_t* create_player( sim_t* sim, const std::string& name, race_e /* r = RACE_NONE */ ) const
  {
    return new enemy_t( sim, name );
  }
  virtual bool valid() const { return true; }
  virtual void init        ( sim_t* ) const {}
  virtual void combat_begin( sim_t* ) const {}
  virtual void combat_end  ( sim_t* ) const {}
};

// HEAL ENEMY MODULE INTERFACE ================================================

struct heal_enemy_module_t : public module_t
{
  heal_enemy_module_t() : module_t( ENEMY ) {}

  virtual player_t* create_player( sim_t* sim, const std::string& name, race_e /* r = RACE_NONE */ ) const
  {
    return new heal_enemy_t( sim, name );
  }
  virtual bool valid() const { return true; }
  virtual void init        ( sim_t* ) const {}
  virtual void combat_begin( sim_t* ) const {}
  virtual void combat_end  ( sim_t* ) const {}
};

} // END UNNAMED NAMESPACE

const module_t& module_t::enemy_()
{
  static enemy_module_t m = enemy_module_t();
  return m;
}

const module_t& module_t::heal_enemy_()
{
  static heal_enemy_module_t m = heal_enemy_module_t();
  return m;
}
